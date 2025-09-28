#include "network_resource_provider.h"
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/system_error.hpp>
#include <boost/url.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "boost_error_code_converter.h"
#include "sanitize_url.h"

namespace
{
#ifdef STV_WINDOWS
#include <wincrypt.h>
void add_windows_root_certs(boost::asio::ssl::context& ctx)
{
    HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
    if (hStore == NULL)
    {
        return;
    }

    X509_STORE* store = X509_STORE_new();
    PCCERT_CONTEXT pContext = NULL;
    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL)
    {
        // convert from DER to internal format
        X509* x509 =
            d2i_X509(NULL, (const unsigned char**)&pContext->pbCertEncoded,
                     pContext->cbCertEncoded);
        if (x509 != NULL)
        {
            X509_STORE_add_cert(store, x509);
            X509_free(x509);
        }
    }

    CertCloseStore(hStore, 0);

    // attach X509_STORE to boost ssl context
    SSL_CTX_set_cert_store(ctx.native_handle(), store);
}
#endif

class RequestSession : public std::enable_shared_from_this<RequestSession>
{
private:
    using response_parser_t =
        boost::beast::http::response_parser<boost::beast::http::string_body>;
    using request_t = boost::beast::http::request<boost::beast::http::empty_body>;
    using stream_t = boost::beast::ssl_stream<boost::beast::tcp_stream>;

public:
    RequestSession(boost::asio::ssl::context& sslContext,
                   const boost::asio::any_io_executor& executor,
                   const HttpProxy& proxy,
                   const std::string& url,
                   NetworkResourceProvider::ResourceLoadedCallback cb)
    : sslContext{ sslContext }
    , executor{ executor }
    , strand{ boost::asio::make_strand(this->executor) }
    , proxy{ proxy }
    , url{ url }
    , cb{ std::move(cb) }
    , stream{ strand, this->sslContext }
    , resolver{ strand }
    {
        auto url_parse_result = boost::urls::parse_uri(url);
        if (!url_parse_result)
        {
            constructRequest(sanitize_uri(url));
        }
        else
        {
            constructRequest(url_parse_result.value());
        }
    }

    void constructRequest(boost::urls::url_view urlView)
    {
        scheme = urlView.has_scheme() ? urlView.scheme() : "http";
        host = urlView.host();
        port = urlView.has_port() ? urlView.port()
                                  : (scheme == "https" ? "443" : "80");
        path = urlView.encoded_target();
        request.version(11);
        request.method(boost::beast::http::verb::get);
        request.target(path);
        request.set(boost::beast::http::field::connection, "close");
        request.set(boost::beast::http::field::host, host);
        request.set(boost::beast::http::field::accept, "*/*");
        request.set(boost::beast::http::field::user_agent, SIMPLEIPTV_STRING);
        request.prepare_payload();
    }

    void PerformRequest()
    {
        if (proxy.use && !proxy.host.empty() && proxy.port > 0)
        {
            resolveProxy();
        }
        else
        {
            resolveHost();
        }
    }

private:
    void resolveProxy()
    {
        boost::system::error_code ec;
        auto results =
            resolver.resolve(proxy.host, std::to_string(proxy.port), ec);
        if (ec)
        {
            handleError(ec);
        }
        else
        {
            connectToProxy(std::move(results));
        }
    }
    void resolveHost()
    {
        boost::system::error_code ec;
        auto results = resolver.resolve(host, scheme, ec);
        if (ec)
        {
            handleError(ec);
        }
        else
        {
            connectToHost(std::move(results));
        }
    }
    void connectToHost(boost::asio::ip::tcp::resolver::results_type results)
    {
        boost::system::error_code ec;
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        stream.next_layer().connect(results, ec);
        if (ec)
        {
            handleError(ec);
            return;
        }
        onConnect();
    }
    void connectToProxy(boost::asio::ip::tcp::resolver::results_type results)
    {
        // we only support http proxies, we can do the connection and protocol
        // synchronously
        boost::system::error_code ec;
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        stream.next_layer().connect(results, ec);
        if (ec)
        {
            handleError(ec);
            return;
        }

        auto dataToWrite = fmt::format("CONNECT {0}:{1} HTTP/1.1\r\n"
                                       "Proxy-Connection: close\r\n"
                                       "Connection: close\r\n"
                                       "Host: {0}:{1}\r\n\r\n",
                                       host, port);
        boost::asio::const_buffer writeBuffer(dataToWrite.data(),
                                              dataToWrite.size());

        boost::asio::write(stream.next_layer(), writeBuffer, ec);
        if (ec)
        {
            handleError(ec);
            return;
        }
        boost::asio::streambuf streambuf;
        boost::asio::read_until(stream.next_layer(), streambuf,
                                std::string_view{ "\r\n\r\n" }, ec);
        if (ec)
        {
            handleError(ec);
        }
        else
        {
            std::string readData;
            std::istream str(&streambuf);
            std::getline(str, readData);
            if (readData.find("HTTP/1.1 2") == std::string::npos)
            {
                handleError(boost::system::errc::make_error_code(
                    boost::system::errc::host_unreachable));
            }
            else
            {
                onConnect();
            }
        }
    }
    void onConnect()
    {
        isSsl = !(scheme == "http" || port == "80" || port == "8080");
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (isSsl &&
            !SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        {
            boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()),
                                         boost::asio::error::get_ssl_category() };
            handleError(ec);
            return;
        }

        if (!isSsl)
        {
            sendRequest();
        }
        else
        {
            boost::beast::error_code ec;
            stream.handshake(boost::asio::ssl::stream_base::client, ec);
            onHandshake(ec);
        }
    }
    void onHandshake(const boost::system::error_code& ec)
    {
        if (ec)
        {
            handleError(ec);
        }
        else
        {
            sendRequest();
        }
    }
    void sendRequest()
    {
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        using namespace std::placeholders;
        // Send the HTTP request to the remote host
        std::size_t bytesTransferred = 0;
        boost::system::error_code ec;
        if (isSsl)
        {
            bytesTransferred = boost::beast::http::write(stream, request, ec);
        }
        else
        {
            bytesTransferred =
                boost::beast::http::write(stream.next_layer(), request, ec);
        }
        onWrite(ec, bytesTransferred);
    }
    void onWrite(const boost::system::error_code& ec, std::size_t bytesTransferred)
    {
        boost::ignore_unused(bytesTransferred);
        if (ec)
        {
            handleError(ec);
            return;
        }
        using namespace std::placeholders;
        auto readMethod =
            std::bind(&RequestSession::onReadHeader, shared_from_this(), _1, _2);
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        boost::system::error_code errorCode;
        std::size_t readHeaderBytesTransferred = 0;
        if (isSsl)
        {
            readHeaderBytesTransferred = boost::beast::http::read_header(
                stream, readBuffer, responseParser, errorCode);
        }
        else
        {
            readHeaderBytesTransferred = boost::beast::http::read_header(
                stream.next_layer(), readBuffer, responseParser, errorCode);
        }
        onReadHeader(errorCode, readHeaderBytesTransferred);
    }
    void onReadHeader(const boost::system::error_code& ec,
                      std::size_t bytesTransferred)
    {
        boost::ignore_unused(bytesTransferred);
        if (ec)
        {
            handleError(ec);
            return;
        }
        std::string location;
        for (const auto& h : responseParser.get())
        {
            if (h.name() == boost::beast::http::field::location)
            {
                location = h.value();
                break;
            }
        }
        if (!location.empty())
        {
            auto status = responseParser.get().result();

            if (status == boost::beast::http::status::moved_permanently ||
                status == boost::beast::http::status::temporary_redirect ||
                status == boost::beast::http::status::permanent_redirect ||
                status == boost::beast::http::status::see_other ||
                status == boost::beast::http::status::found)
            {
                // close this, make a new request session for the new location
                boost::asio::post(
                    executor,
                    [self = shared_from_this(), location]()
                    {
                        auto session = std::make_shared<RequestSession>(
                            self->sslContext, self->executor, self->proxy,
                            location, std::move(self->cb));
                        session->PerformRequest();
                    });
            }
        }
        else
        {
            boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
            boost::system::error_code errorCode;
            std::size_t readBytesTransferred = 0;
            if (isSsl)
            {
                readBytesTransferred = boost::beast::http::read(
                    stream, readBuffer, responseParser, errorCode);
            }
            else
            {
                readBytesTransferred = boost::beast::http::read(
                    stream.next_layer(), readBuffer, responseParser, errorCode);
            }
            onRead(errorCode, readBytesTransferred);
        }
    }
    void onRead(const boost::system::error_code& ec, std::size_t bytesTransferred)
    {
        boost::ignore_unused(bytesTransferred);
        if (ec)
        {
            handleError(ec);
            return;
        }
        // we got our response
        std::string body = responseParser.get().body();
        cb(std::move(body), std::error_code{});
    }

    void handleError(const boost::system::error_code& ec)
    {
        cb("", BoostToErrorCode(ec));
    }

private:
    boost::asio::ssl::context& sslContext;
    boost::asio::any_io_executor executor;
    boost::asio::strand<boost::asio::any_io_executor> strand;
    HttpProxy proxy;
    std::string url;
    NetworkResourceProvider::ResourceLoadedCallback cb;
    stream_t stream;
    response_parser_t responseParser;
    request_t request;
    boost::beast::flat_buffer readBuffer;
    boost::asio::ip::tcp::resolver resolver;
    std::chrono::seconds streamTimeout{ 10 };
    std::string host;
    std::string port;
    std::string path;
    std::string scheme;
    bool isSsl = false;
};

} // namespace

NetworkResourceProvider::NetworkResourceProvider(
    Key,
    const boost::asio::any_io_executor& executor,
    std::shared_ptr<ProxyRepository> proxyRepository)
: executor{ executor }
, proxyRepository{ std::move(proxyRepository) }
, sslContext{ boost::asio::ssl::context::tls_client }
{
    sslContext.set_options(boost::asio::ssl::context::default_workarounds);
    sslContext.set_verify_mode(boost::asio::ssl::verify_peer);
#ifdef STV_WINDOWS
    add_windows_root_certs(sslContext);
#else
    sslContext.set_default_verify_paths();
#endif
}
std::shared_ptr<NetworkResourceProvider>
NetworkResourceProvider::Create(const boost::asio::any_io_executor& executor,
                                std::shared_ptr<ProxyRepository> proxyRepository)
{
    return std::make_shared<NetworkResourceProvider>(Key{}, executor,
                                                     std::move(proxyRepository));
}
void NetworkResourceProvider::GetResource(
    const std::string& url,
    const boost::asio::any_io_executor& cb_executor,
    ResourceLoadedCallback cb,
    bool cacheResource)
{
    using namespace std::placeholders;
    proxyRepository->LoadConfiguredProxy(
        [weak = weak_from_this(), url, cb_executor, cb,
         cacheResource](HttpProxy proxy)
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->getResource(std::move(proxy), std::move(url),
                              std::move(cb_executor), std::move(cb),
                              cacheResource);
        },
        executor);
}
void NetworkResourceProvider::getResource(HttpProxy proxy,
                                          std::string url,
                                          boost::asio::any_io_executor cb_executor,
                                          ResourceLoadedCallback cb,
                                          bool cacheResource)
{
    try
    {
        auto logo = cacheResource ? cache.Get(url) : std::optional<std::string>{};
        if (cacheResource && logo)
        {
            spdlog::debug("Got {} from cache", url);
            auto func = std::bind(cb, logo.value(), std::error_code{});
            boost::asio::post(cb_executor, func);
        }
        else
        {
            spdlog::debug("Downloading {}", url);
            auto request = std::make_shared<RequestSession>(
                sslContext, executor, proxy, url,
                [weak = weak_from_this(), url, cb_executor, cb,
                 cacheResource](std::string body, std::error_code ec)
                {
                    auto self = weak.lock();
                    if (!self)
                        return;
                    if (!ec && cacheResource)
                    {
                        self->cache.Put(std::move(url), body);
                    }
                    auto func = std::bind(cb, std::move(body), ec);
                    boost::asio::post(cb_executor, func);
                });
            request->PerformRequest();
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Cannot perform request {}", ex.what());
    }
}
