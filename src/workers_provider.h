#pragma once

#include <boost/asio/thread_pool.hpp>

#include "channels/channels_repository.h"
#include "proxy_repository.h"

class WorkersProvider
{
public:
    WorkersProvider();
    ~WorkersProvider();
    std::shared_ptr<ChannelsRepository> GetChannelsRepository();
    std::shared_ptr<ProxyRepository> GetProxyRepository();

private:
    boost::asio::thread_pool io_pool;
};