#pragma once

#include <boost/asio/thread_pool.hpp>

#include "channels/channels_repository.h"

class WorkersProvider
{
public:
    WorkersProvider();
    ~WorkersProvider();
    std::shared_ptr<ChannelsRepository> GetChannelsRepository();

private:
    boost::asio::thread_pool io_pool;
};