#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <functional>
#include <memory>
#include <optional>

#include "proxy_repository.h"

class NetworkResourceProvider
: public std::enable_shared_from_this<NetworkResourceProvider>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    NetworkResourceProvider(Key,
                            const boost::asio::any_io_executor& executor,
                            std::shared_ptr<ProxyRepository> proxyRepository);
    static std::shared_ptr<NetworkResourceProvider>
    Create(const boost::asio::any_io_executor& executor,
           std::shared_ptr<ProxyRepository> proxyRepository);

    using ResourceLoadedCallback =
        std::function<void(std::string, std::error_code)>;
    // the callback will be called on the cb_executor provided
    void GetResource(const std::string& url,
                     const boost::asio::any_io_executor& cb_executor,
                     ResourceLoadedCallback cb);

private:
    void getResource(HttpProxy proxy,
                     std::string url,
                     boost::asio::any_io_executor cb_executor,
                     ResourceLoadedCallback cb);

private:
    boost::asio::any_io_executor executor;
    std::shared_ptr<ProxyRepository> proxyRepository;
    boost::asio::ssl::context sslContext;
};