#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/thread_pool.hpp>

#include "channels/channels_repository.h"
#include "dbus_service.h"
#include "proxy_repository.h"

class WorkersProvider
{
public:
    WorkersProvider();
    ~WorkersProvider();
    std::shared_ptr<ChannelsRepository> GetChannelsRepository();
    std::shared_ptr<ProxyRepository> GetProxyRepository();
    boost::asio::any_io_executor GetWorkersExecutor();
    std::shared_ptr<DBusService> GetDBusService();

private:
    boost::asio::thread_pool io_pool;
    std::shared_ptr<ChannelsRepository> channelsRepository;
    std::shared_ptr<ProxyRepository> proxyRepository;
    std::shared_ptr<DBusService> dbusService;
};