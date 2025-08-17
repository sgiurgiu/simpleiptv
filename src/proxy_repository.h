#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <functional>
#include <memory>

struct HttpProxy
{
    std::string host;
    int port = 0;
    bool use = false;
};

class ProxyRepository : public std::enable_shared_from_this<ProxyRepository>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ProxyRepository(Key, const boost::asio::any_io_executor& executor);
    static std::shared_ptr<ProxyRepository>
    Create(const boost::asio::any_io_executor& executor);

    using LoadProxyCallback = std::function<void(HttpProxy)>;
    // the callback will be called on the cb_executor provided
    void LoadConfiguredProxy(LoadProxyCallback cb,
                             const boost::asio::any_io_executor& cb_executor);
    void SaveConfiguredProxy(HttpProxy proxy);

    template <typename S>
    void AddUpdatedProxySignalListener(S signal)
    {
        proxySettingsSignal.connect(signal);
    }

private:
    boost::asio::any_io_executor executor;
    using UpdatedProxySettingsSignal = boost::signals2::signal<void(HttpProxy)>;
    UpdatedProxySettingsSignal proxySettingsSignal;
};