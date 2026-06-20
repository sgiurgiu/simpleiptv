#include "network_resource_provider.h"
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/system_error.hpp>
#include <boost/url.hpp>
#include <charconv>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <system_error>

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
template <typename Derived, typename ResponseBody>
class RequestSessionBase : public std::enable_shared_from_this<Derived>
{
protected:
    static constexpr int kMaxRedirects = 10;
    using response_parser_t = boost::beast::http::response_parser<ResponseBody>;
    using request_t = boost::beast::http::request<boost::beast::http::empty_body>;
    using stream_t = boost::beast::ssl_stream<boost::beast::tcp_stream>;

public:
    RequestSessionBase(boost::asio::ssl::context& sslContext,
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

    virtual ~RequestSessionBase() = default;

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
            std::string statusLine;
            std::istream str(&streambuf);
            std::getline(str, statusLine);
            // Status line is "HTTP/x.y <code> <reason>". Accept any 2xx
            // tunnel reply regardless of the proxy's HTTP version.
            int statusCode = 0;
            if (auto sp = statusLine.find(' '); sp != std::string::npos)
            {
                std::from_chars(statusLine.data() + sp + 1,
                                statusLine.data() + statusLine.size(),
                                statusCode);
            }
            if (statusCode < 200 || statusCode >= 300)
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
        isSsl = (scheme == "https");
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
        auto status = responseParser.get().result();
        if (!location.empty())
        {
            if (status == boost::beast::http::status::moved_permanently ||
                status == boost::beast::http::status::temporary_redirect ||
                status == boost::beast::http::status::permanent_redirect ||
                status == boost::beast::http::status::see_other ||
                status == boost::beast::http::status::found)
            {
                onLocationChanged(std::move(location));
            }
        }
        else
        {
            if (status == boost::beast::http::status::ok)
            {
                readBody();
            }
            else
            {
                handleError({ (int)boost::beast::http::error::bad_status,
                              ec.category() });
            }
        }
    }

protected:
    void handleError(const boost::system::error_code& ec)
    {
        cb("", BoostToErrorCode(ec));
    }

protected:
    // Close this session and start a fresh one for the redirect target.
    // The Location header may be relative, so resolve it against the URL we
    // originally requested. Bail out once we have followed too many hops.
    void onLocationChanged(std::string location)
    {
        boost::asio::post(
            executor,
            [self = this->shared_from_this(), location = std::move(location)]() mutable
            {
                if (self->redirects >= kMaxRedirects)
                {
                    self->handleError(boost::system::errc::make_error_code(
                        boost::system::errc::too_many_links));
                    return;
                }
                auto base = boost::urls::parse_uri(self->url);
                auto ref = boost::urls::parse_uri_reference(location);
                if (base && ref)
                {
                    boost::urls::url resolved;
                    if (boost::urls::resolve(base.value(), ref.value(), resolved))
                    {
                        location = resolved.buffer();
                    }
                }
                auto session = std::make_shared<Derived>(
                    self->sslContext, self->executor, self->proxy,
                    std::move(location), std::move(self->cb));
                session->redirects = self->redirects + 1;
                session->PerformRequest();
            });
    }
    virtual void readBody() = 0;

protected:
    int redirects = 0;
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

class RequestSession
: public RequestSessionBase<RequestSession, boost::beast::http::string_body>
{
public:
    using RequestSessionBase::RequestSessionBase;

private:
    void readBody() override
    {
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        boost::system::error_code errorCode;
        if (isSsl)
        {
            boost::beast::http::read(stream, readBuffer, responseParser,
                                     errorCode);
        }
        else
        {
            boost::beast::http::read(stream.next_layer(), readBuffer,
                                     responseParser, errorCode);
        }

        if (errorCode)
        {
            handleError(errorCode);
            return;
        }
        // we got our response
        std::string body = responseParser.get().body();
        cb(std::move(body), std::error_code{});
    }
};

class RequestSessionStream
: public RequestSessionBase<RequestSessionStream, boost::beast::http::buffer_body>
{
public:
    using RequestSessionBase::RequestSessionBase;

private:
    void readBody() override
    {
        responseParser.body_limit(boost::none);
        while (!responseParser.is_done())
        {
            char buffer[4 * 1024];
            responseParser.get().body().data = buffer;
            responseParser.get().body().size = sizeof(buffer);
            boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
            boost::system::error_code errorCode;
            if (isSsl)
            {
                boost::beast::http::read(stream, readBuffer, responseParser,
                                         errorCode);
            }
            else
            {
                boost::beast::http::read(stream.next_layer(), readBuffer,
                                         responseParser, errorCode);
            }

            if (errorCode == boost::beast::http::error::need_buffer)
            {
                errorCode.assign(0, errorCode.category());
            }

            if (errorCode)
            {
                handleError(errorCode);
                return;
            }
            std::size_t bodyBytes =
                sizeof(buffer) - responseParser.get().body().size;
            if (bodyBytes > 0)
            {
                // we got our response
                std::string body(buffer, bodyBytes);
                cb(std::move(body), std::error_code{});
            }
        }
        // end of stream
        cb("", std::error_code{});
    }
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
        [weak = weak_from_this(), url, cb_executor, cb = std::move(cb),
         cacheResource](HttpProxy proxy) mutable
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
        auto cachedResource =
            cacheResource ? cache.Get(url) : std::optional<std::string>{};
        if (cacheResource && cachedResource)
        {
            spdlog::debug("Got {} from cache", url);
            auto func = std::bind(cb, cachedResource.value(), std::error_code{});
            boost::asio::post(cb_executor, func);
        }
        else
        {
            spdlog::debug("Downloading {}", url);
            auto request = std::make_shared<RequestSession>(
                sslContext, executor, proxy, url,
                [weak = weak_from_this(), url, cb_executor, cb = std::move(cb),
                 cacheResource](std::string body, std::error_code ec) mutable
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
        auto func = std::bind(cb, std::string{},
                              std::make_error_code(std::errc::io_error));
        boost::asio::post(cb_executor, func);
    }
}

void NetworkResourceProvider::GetResourceStreaming(
    const std::string& url,
    const boost::asio::any_io_executor& cb_executor,
    ResourceLoadedCallback cb)
{
    using namespace std::placeholders;
    proxyRepository->LoadConfiguredProxy(
        [weak = weak_from_this(), url, cb_executor,
         cb = std::move(cb)](HttpProxy proxy) mutable
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->getResourceStreaming(std::move(proxy), std::move(url),
                                       std::move(cb_executor), std::move(cb));
        },
        executor);
}
void NetworkResourceProvider::getResourceStreaming(
    HttpProxy proxy,
    std::string url,
    boost::asio::any_io_executor cb_executor,
    ResourceLoadedCallback cb)
{
    try
    {
        spdlog::debug("Downloading {}", url);
        auto request = std::make_shared<RequestSessionStream>(
            sslContext, executor, proxy, url,
            [url, cb_executor, cb = std::move(cb)](std::string body,
                                                   std::error_code ec) mutable
            {
                auto func = std::bind(cb, std::move(body), ec);
                boost::asio::post(cb_executor, func);
            });
        request->PerformRequest();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Cannot perform request {}", ex.what());
        auto func = std::bind(cb, std::string{},
                              std::make_error_code(std::errc::io_error));
        boost::asio::post(cb_executor, func);
    }
}
