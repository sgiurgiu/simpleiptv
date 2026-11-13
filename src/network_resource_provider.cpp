#include "network_resource_provider.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
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
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <system_error>
#include <unordered_set>

#include "boost_error_code_converter.h"
#include "sanitize_url.h"

// The private io_context of every request currently running, so Stop() can
// interrupt them all from the shutdown thread.
class ActiveNetworkRequests
{
public:
    // false once StopAll() has been called: the request must not start
    bool Add(boost::asio::io_context* ioc)
    {
        std::lock_guard<std::mutex> _{ mutex };
        if (stopping)
        {
            return false;
        }
        contexts.insert(ioc);
        return true;
    }
    void Remove(boost::asio::io_context* ioc)
    {
        std::lock_guard<std::mutex> _{ mutex };
        contexts.erase(ioc);
    }
    bool IsStopping()
    {
        std::lock_guard<std::mutex> _{ mutex };
        return stopping;
    }
    void StopAll()
    {
        std::lock_guard<std::mutex> _{ mutex };
        stopping = true;
        for (auto* ioc : contexts)
        {
            ioc->stop();
        }
    }

private:
    std::mutex mutex;
    std::unordered_set<boost::asio::io_context*> contexts;
    bool stopping = false;
};

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
                       std::shared_ptr<ActiveNetworkRequests> activeRequests,
                       const HttpProxy& proxy,
                       const std::string& url,
                       NetworkResourceProvider::ResourceLoadedCallback cb)
    : sslContext{ sslContext }
    , executor{ executor }
    , activeRequests{ std::move(activeRequests) }
    , proxy{ proxy }
    , url{ url }
    , cb{ std::move(cb) }
    , stream{ ioc, this->sslContext }
    , resolver{ ioc }
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

    // Blocks the calling network-pool thread until the request is done, so
    // the pool size stays the cap on concurrent downloads (thousands of logos
    // get requested on a first load).
    void PerformRequest()
    {
        // ioc is registered by address below, so this session must outlive the
        // registration. Hold our own reference rather than rely on the caller's.
        // `self` is declared first, so it is released only after `unregister`
        // has run.
        auto self = this->shared_from_this();
        if (!activeRequests->Add(&ioc))
        {
            return; // shutting down
        }
        // Remove() takes the same lock as StopAll(), so once it returns no
        // StopAll() can still be using &ioc.
        struct Unregister
        {
            ActiveNetworkRequests& requests;
            boost::asio::io_context& ioc;
            ~Unregister()
            {
                requests.Remove(&ioc);
            }
        } unregister{ *activeRequests, ioc };

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
        // port, not scheme: the scheme only maps to 80/443, which would
        // ignore an explicit port in the URL (e.g. http://host:8080/...)
        auto results = resolver.resolve(host, port, ec);
        if (ec)
        {
            handleError(ec);
        }
        else
        {
            connectToHost(std::move(results));
        }
    }
    boost::system::error_code
    connect(const boost::asio::ip::tcp::resolver::results_type& results)
    {
        auto& tcp = boost::beast::get_lowest_layer(stream);
        tcp.expires_after(streamTimeout);
        return runOperation([&](auto handler)
                            { tcp.async_connect(results, std::move(handler)); });
    }
    void connectToHost(boost::asio::ip::tcp::resolver::results_type results)
    {
        auto ec = connect(results);
        if (ec)
        {
            handleError(ec);
            return;
        }
        onConnect();
    }
    void connectToProxy(boost::asio::ip::tcp::resolver::results_type results)
    {
        // we only support http proxies: open a tunnel with CONNECT, then
        // carry on over it as if we were talking to the host directly
        auto ec = connect(results);
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
        auto& tcp = boost::beast::get_lowest_layer(stream);
        tcp.expires_after(streamTimeout);
        ec = runOperation(
            [&](auto handler)
            {
                boost::asio::async_write(tcp, boost::asio::buffer(dataToWrite),
                                         std::move(handler));
            });
        if (ec)
        {
            handleError(ec);
            return;
        }
        boost::asio::streambuf streambuf;
        tcp.expires_after(streamTimeout);
        ec = runOperation(
            [&](auto handler)
            {
                boost::asio::async_read_until(tcp, streambuf,
                                              std::string_view{ "\r\n\r\n" },
                                              std::move(handler));
            });
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
            boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
            auto ec = runOperation(
                [&](auto handler)
                {
                    stream.async_handshake(boost::asio::ssl::stream_base::client,
                                           std::move(handler));
                });
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
        // Send the HTTP request to the remote host
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        auto ec = runOperation(
            [&](auto handler)
            {
                if (isSsl)
                {
                    boost::beast::http::async_write(stream, request,
                                                    std::move(handler));
                }
                else
                {
                    boost::beast::http::async_write(stream.next_layer(), request,
                                                    std::move(handler));
                }
            });
        onWrite(ec);
    }
    void onWrite(const boost::system::error_code& ec)
    {
        if (ec)
        {
            handleError(ec);
            return;
        }
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        auto errorCode = runOperation(
            [&](auto handler)
            {
                if (isSsl)
                {
                    boost::beast::http::async_read_header(
                        stream, readBuffer, responseParser, std::move(handler));
                }
                else
                {
                    boost::beast::http::async_read_header(
                        stream.next_layer(), readBuffer, responseParser,
                        std::move(handler));
                }
            });
        onReadHeader(errorCode);
    }

    void onReadHeader(const boost::system::error_code& ec)
    {
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
    /**
     * Runs one async operation on this request's private io_context and waits
     * for it to finish. The request still reads top to bottom like
     * synchronous code, but unlike the synchronous calls, async operations
     * honour the stream's expires_after() timeouts, and Stop() can interrupt
     * them from another thread.
     *
     * If Stop() interrupts the operation, its handler is left pending and
     * still refers to `result` on this stack frame. Every caller treats the
     * returned error as fatal, so the io_context is never run again and the
     * handler is only ever destroyed, never invoked.
     */
    template <typename Initiation>
    boost::system::error_code runOperation(Initiation&& initiation)
    {
        ioc.restart();
        // Checked after restart() so a Stop() that lands between two
        // operations isn't wiped out by it.
        if (activeRequests->IsStopping())
        {
            return boost::asio::error::operation_aborted;
        }
        std::optional<boost::system::error_code> result;
        initiation([&result](boost::system::error_code ec, auto&&...)
                   { result = ec; });
        ioc.run();
        return result.value_or(boost::asio::error::operation_aborted);
    }

    // Reads some of the body, waiting at most streamTimeout for it: a long
    // download is fine as long as data keeps arriving.
    boost::system::error_code readSomeBody()
    {
        boost::beast::get_lowest_layer(stream).expires_after(streamTimeout);
        return runOperation(
            [&](auto handler)
            {
                if (isSsl)
                {
                    boost::beast::http::async_read_some(
                        stream, readBuffer, responseParser, std::move(handler));
                }
                else
                {
                    boost::beast::http::async_read_some(
                        stream.next_layer(), readBuffer, responseParser,
                        std::move(handler));
                }
            });
    }

    void handleError(const boost::system::error_code& ec)
    {
        // Aborted by Stop(): the app is shutting down and nobody is waiting
        // for the result.
        if (activeRequests->IsStopping())
        {
            return;
        }
        cb("", BoostToErrorCode(ec));
    }

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
                    self->sslContext, self->executor, self->activeRequests,
                    self->proxy, std::move(location), std::move(self->cb));
                session->redirects = self->redirects + 1;
                session->PerformRequest();
            });
    }
    virtual void readBody() = 0;

protected:
    int redirects = 0;
    boost::asio::ssl::context& sslContext;
    // The network pool; only used to hand a redirect over to a new session.
    boost::asio::any_io_executor executor;
    std::shared_ptr<ActiveNetworkRequests> activeRequests;
    HttpProxy proxy;
    std::string url;
    NetworkResourceProvider::ResourceLoadedCallback cb;
    // Private to this request and run only by PerformRequest's thread.
    // Declared before the I/O objects that use it, so it outlives them.
    boost::asio::io_context ioc;
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
        while (!responseParser.is_done())
        {
            auto errorCode = readSomeBody();
            if (errorCode)
            {
                handleError(errorCode);
                return;
            }
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
            auto errorCode = runOperation(
                [&](auto handler)
                {
                    if (isSsl)
                    {
                        boost::beast::http::async_read(stream, readBuffer,
                                                       responseParser,
                                                       std::move(handler));
                    }
                    else
                    {
                        boost::beast::http::async_read(stream.next_layer(),
                                                       readBuffer, responseParser,
                                                       std::move(handler));
                    }
                });

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
, activeRequests{ std::make_shared<ActiveNetworkRequests>() }
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

void NetworkResourceProvider::Stop()
{
    activeRequests->StopAll();
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
                sslContext, executor, activeRequests, proxy, url,
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
            sslContext, executor, activeRequests, proxy, url,
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
