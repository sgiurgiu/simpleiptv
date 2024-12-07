#pragma once

#include "channels_window.h"
#include "mpvplayer.h"

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext);
    void setSize(int width, int height);
    void showDesktop();

    bool shouldQuit() const
    {
        return channels.shouldQuit() || quit;
    }

private:
    boost::asio::any_io_executor ui_executor;
    ChannelsWindow channels;
    MpvPlayer player;
    int width = 0;
    int height = 0;
    bool quit = false;
};