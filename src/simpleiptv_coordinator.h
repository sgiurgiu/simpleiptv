#pragma once

#include "simpleiptv_ui.h"
#include "mpvplayer.h"
#include "workers_provider.h"
#include "simpleiptv_vulkan.h"
#include <boost/asio/io_context.hpp>

#ifdef STV_UNIX
#include "dbus_mpris_service.h"
#endif

class SimpleIPTVCoordinator
{
public:
    SimpleIPTVCoordinator(boost::asio::io_context& uiContext,
                          WorkersProvider* workersProvider,
                          SimpleIPTVVulkan* vulkanInstance);
    void Render();
    void SetSize(int width, int height);
    bool ShouldQuit() const;

private:
    WorkersProvider* workersProvider;
    SimpleIPTVUI simpleiptv;
    MpvPlayer mpvPlayer;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif

};