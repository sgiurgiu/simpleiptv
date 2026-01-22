#pragma once

#include <boost/asio/steady_timer.hpp>
#include <mutex>

#include "channels_window.h"
#include "epg_listings_window.h"
#include "mpvplayer.h"
#include "playerbar_window.h"
#include "simpleiptv_vulkan.h"
#include "workers_provider.h"
#include <imgui_internal.h>

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext,
               WorkersProvider* workersProvider,
               SimpleIPTVVulkan* vulkanInstance,
               std::mutex* imguiRenderMutex);
    void setSize(int width, int height);
    ImRect showDesktop();
    void Render(const ImRect& desktopRect);
    bool shouldQuit() const
    {
        return channelsWindow->ShouldQuit() || quit;
    }

private:
    void channelActivated(ChannelPtr channel);
    void rearmChannelsShowingTimer();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    SimpleIPTVVulkan* vulkanInstance;
    std::shared_ptr<ChannelsWindow> channelsWindow;
    std::shared_ptr<PlayerBarWindow> playerBarWindow;
    std::shared_ptr<EpgListingWindow> epgListingWindow;
    MpvPlayer player;
    int width = 0;
    int height = 0;
    bool quit = false;
    boost::asio::steady_timer channelsShowingTimer;
    bool showChannels = true;
    bool needsResize = false;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif
};