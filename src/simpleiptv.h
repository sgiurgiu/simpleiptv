#pragma once

#include <boost/asio/steady_timer.hpp>
#include <chrono>

#include "channels_window.h"
#include "mpvplayer.h"
#include "playerbar_window.h"
#include "workers_provider.h"

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext,
               WorkersProvider* workersProvider);
    void setSize(int width, int height);
    void showDesktop();

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
    std::shared_ptr<ChannelsWindow> channelsWindow;
    PlayerBarWindow playerBarWindow;
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