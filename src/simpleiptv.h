#pragma once

#include <boost/asio/steady_timer.hpp>
#include <chrono>

#include "channels_window.h"
#include "epg_listings_window.h"
#include "mpvplayer.h"
#include "playerbar_window.h"
#include "simpleiptv_vulkan.h"
#include "workers_provider.h"

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext,
               WorkersProvider* workersProvider,
               SimpleIPTVVulkan* vulkanInstance);
    void setSize(int width, int height);
    ImVec2 showDesktop();
    void Render(const ImVec2& windowSize);
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
    std::chrono::steady_clock::time_point lastResizeTime;
    std::chrono::steady_clock::time_point lastActivityTime;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif
};