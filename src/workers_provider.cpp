#include "workers_provider.h"

#include <thread>

WorkersProvider::WorkersProvider()
: networkPool{ 6 }
, dbPool{ 1 }
, channelsRepository{ ChannelsRepository::Create(dbPool.get_executor()) }
, proxyRepository{ ProxyRepository::Create(dbPool.get_executor()) }
, sleepService{ SleepService::Create(networkPool.get_executor()) }
, settingsRepository{ SettingsRepository::Create() }
, networkResourceProvider{ NetworkResourceProvider::Create(
      networkPool.get_executor(), proxyRepository) }
#ifdef STV_UNIX
, mprisService{ MprisService::Create() }
#endif

{
}

std::shared_ptr<ChannelsRepository> WorkersProvider::GetChannelsRepository()
{
    return channelsRepository;
}
std::shared_ptr<ProxyRepository> WorkersProvider::GetProxyRepository()
{
    return proxyRepository;
}
boost::asio::any_io_executor WorkersProvider::GetNetworkExecutor()
{
    return networkPool.get_executor();
}
boost::asio::any_io_executor WorkersProvider::GetDBExecutor()
{
    return dbPool.get_executor();
}
std::shared_ptr<SleepService> WorkersProvider::GetSleepService()
{
    return sleepService;
}
std::shared_ptr<SettingsRepository> WorkersProvider::GetSettingsRepository()
{
    return settingsRepository;
}
std::shared_ptr<NetworkResourceProvider>
WorkersProvider::GetNetworkResourceProvider()
{
    return networkResourceProvider;
}
#ifdef STV_UNIX
std::shared_ptr<MprisService> WorkersProvider::GetMprisService()
{
    return mprisService;
}
#endif