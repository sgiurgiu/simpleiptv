#include "workers_provider.h"

WorkersProvider::WorkersProvider() : io_pool{ 6 }
{
}
WorkersProvider::~WorkersProvider()
{
    io_pool.stop();
    io_pool.join();
}
std::shared_ptr<ChannelsRepository> WorkersProvider::GetChannelsRepository()
{
    return ChannelsRepository::Create(io_pool.get_executor());
}
std::shared_ptr<ProxyRepository> WorkersProvider::GetProxyRepository()
{
    return ProxyRepository::Create(io_pool.get_executor());
}