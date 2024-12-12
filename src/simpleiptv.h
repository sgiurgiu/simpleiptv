#pragma once

#include "channels_window.h"
#include "mpvplayer.h"
#include "workers_provider.h"

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext,
               WorkersProvider& workersProvider);
    void setSize(int width, int height);
    void showDesktop();

    bool shouldQuit() const
    {
        return channelsWindow->shouldQuit() || quit;
    }

private:
    void channelActivated(ChannelPtr channel);

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider& workersProvider;
    std::shared_ptr<ChannelsWindow> channelsWindow;
    MpvPlayer player;
    int width = 0;
    int height = 0;
    bool quit = false;
};