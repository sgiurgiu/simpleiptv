#include "workers_provider.h"

WorkersProvider::WorkersProvider()
: io_pool{ 6 }
, channelsRepository{ ChannelsRepository::Create(io_pool.get_executor()) }
, proxyRepository{ ProxyRepository::Create(io_pool.get_executor()) }
, sleepService{ SleepService::Create(io_pool.get_executor()) }
, settingsRepository{ SettingsRepository::Create() }
#ifdef STV_UNIX
, mprisService{ MprisService::Create() }
#endif

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
std::shared_ptr<SleepService> WorkersProvider::GetSleepService()
{
    return sleepService;
}
std::shared_ptr<SettingsRepository> WorkersProvider::GetSettingsRepository()
{
    return settingsRepository;
}
#ifdef STV_UNIX
std::shared_ptr<MprisService> WorkersProvider::GetMprisService()
{
    return mprisService;
}
#endif