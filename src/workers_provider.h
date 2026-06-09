#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/thread_pool.hpp>

#include "channels/channels_repository.h"
#include "epg/epg_repository.h"
#include "network_resource_provider.h"
#include "proxy_repository.h"
#include "servers/servers_repository.h"
#include "settings_repository.h"
#include "sleep_service.h"

#ifdef STV_UNIX
#include "dbus_mpris_service.h"
#endif

class WorkersProvider
{
public:
    WorkersProvider();
    // Stops the D-Bus service and joins the worker pools. Call before tearing
    // down the UI io_context so no worker/callback posts into a dead context.
    void StopWorkers();
    std::shared_ptr<ChannelsRepository> GetChannelsRepository();
    std::shared_ptr<ProxyRepository> GetProxyRepository();
    boost::asio::any_io_executor GetNetworkExecutor();
    boost::asio::any_io_executor GetDBExecutor();
    std::shared_ptr<SleepService> GetSleepService();
    std::shared_ptr<SettingsRepository> GetSettingsRepository();
    std::shared_ptr<NetworkResourceProvider> GetNetworkResourceProvider();
    std::shared_ptr<ServersRepository> GetServersRepository();
    std::shared_ptr<EpgRepository> GetEpgRepository();
#ifdef STV_UNIX
    std::shared_ptr<MprisService> GetMprisService();
#endif

private:
    boost::asio::thread_pool networkPool;
    boost::asio::thread_pool dbPool;
    std::shared_ptr<ChannelsRepository> channelsRepository;
    std::shared_ptr<ProxyRepository> proxyRepository;
    std::shared_ptr<SleepService> sleepService;
    std::shared_ptr<SettingsRepository> settingsRepository;
    std::shared_ptr<NetworkResourceProvider> networkResourceProvider;
    std::shared_ptr<ServersRepository> serversRepository;
    std::shared_ptr<EpgRepository> epgRepository;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif
};