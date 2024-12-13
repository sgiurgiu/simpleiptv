#include "workers_provider.h"

WorkersProvider::WorkersProvider()
: io_pool{ 6 }
, channelsRepository{ ChannelsRepository::Create(io_pool.get_executor()) }
, proxyRepository{ ProxyRepository::Create(io_pool.get_executor()) }
, dbusService{ DBusService::Create(io_pool.get_executor()) }
{
}
WorkersProvider::~WorkersProvider()
{
    io_pool.stop();
    io_pool.join();
}
std::shared_ptr<ChannelsRepository> WorkersProvider::GetChannelsRepository()
{
    return channelsRepository;
}
std::shared_ptr<ProxyRepository> WorkersProvider::GetProxyRepository()
{
    return proxyRepository;
}
boost::asio::any_io_executor WorkersProvider::GetWorkersExecutor()
{
    return io_pool.get_executor();
}
std::shared_ptr<DBusService> WorkersProvider::GetDBusService()
{
    return dbusService;
}