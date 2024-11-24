#pragma once

#include "channels_window.h"
#include "mpvplayer.h"

class SimpleIPTV
{
public:
    SimpleIPTV(boost::asio::io_context& uiContext);
    void setSize(int width, int height);
    void showDesktop();

private:
    boost::asio::any_io_executor ui_executor;
    ChannelsWindow channels;
    MpvPlayer player;
    int width = 0;
    int height = 0;
};