#include "sleep_service.h"

#include <spdlog/spdlog.h>

#ifdef STV_UNIX
#include <dbus/org.freedesktop.ScreenSaver.xml.h>
#endif

namespace
{
#ifdef STV_UNIX
class ScreenSaverProxy final
: public sdbus::ProxyInterfaces<org::freedesktop::ScreenSaver_proxy>
{
public:
    ScreenSaverProxy(sdbus::IConnection& connection,
                     sdbus::ServiceName destination,
                     sdbus::ObjectPath path)
    : ProxyInterfaces(connection, std::move(destination), std::move(path))
    {
        registerProxy();
    }

    ~ScreenSaverProxy()
    {
        unregisterProxy();
    }

    void onActiveChanged(const bool&)
    {
    }
};
#endif
} // namespace

SleepService::SleepService(Key, const boost::asio::any_io_executor& executor)
: executor{ executor }, sessionConnection{ sdbus::createSessionBusConnection() }
{
}
std::shared_ptr<SleepService>
SleepService::Create(const boost::asio::any_io_executor& executor)
{
    return std::make_shared<SleepService>(Key{}, executor);
}

void SleepService::disableComputerSleep()
{
    spdlog::debug("disabling computer sleep");
    setComputerSleep(false);
}
void SleepService::enableComputerSleep()
{
    spdlog::debug("enabling computer sleep");
    setComputerSleep(true);
}
void SleepService::setComputerSleep(bool flag)
{
#ifdef STV_UNIX
    try
    {
        sdbus::ServiceName destination{ "org.freedesktop.ScreenSaver" };
        sdbus::ObjectPath objectPath{ "/org/freedesktop/ScreenSaver" };

        auto screenSaverProxy = std::make_unique<ScreenSaverProxy>(
            *sessionConnection, std::move(destination), std::move(objectPath));

        if (!flag)
        {
            screenSaverDBusCookie =
                screenSaverProxy->Inhibit("simpleiptv", "playing video");
        }
        else
        {
            screenSaverProxy->UnInhibit(screenSaverDBusCookie);
        }
    }
    catch (const sdbus::Error& er)
    {
        spdlog::error("Error making dbus call to ScreenSaver service: {} - {}",
                      er.getName(), er.getMessage());
    }

#elif defined STV_WIN

    EXECUTION_STATE result;

    if (!flag)
        result = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                                         ES_DISPLAY_REQUIRED);
    else
        result = SetThreadExecutionState(ES_CONTINUOUS);

    if (result == nullptr)
        spdlog::debug("EXECUTION_STATE failed");

#endif
}
