#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/thread_pool.hpp>

#include "channels/channels_repository.h"
#include "proxy_repository.h"
#include "settings_repository.h"
#include "sleep_service.h"

#ifdef STV_UNIX
#include "dbus_mpris_service.h"
#endif

class WorkersProvider
{
public:
    WorkersProvider();
    ~WorkersProvider();
    std::shared_ptr<ChannelsRepository> GetChannelsRepository();
    std::shared_ptr<ProxyRepository> GetProxyRepository();
    boost::asio::any_io_executor GetWorkersExecutor();
    std::shared_ptr<SleepService> GetSleepService();
    std::shared_ptr<SettingsRepository> GetSettingsRepository();
#ifdef STV_UNIX
    std::shared_ptr<MprisService> GetMprisService();
#endif

private:
    boost::asio::thread_pool io_pool;
    std::shared_ptr<ChannelsRepository> channelsRepository;
    std::shared_ptr<ProxyRepository> proxyRepository;
    std::shared_ptr<SleepService> sleepService;
    std::shared_ptr<SettingsRepository> settingsRepository;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif
};