#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/thread_pool.hpp>

#include "serverpopup.h"

class ChannelsWindow
{
public:
    ChannelsWindow(const boost::asio::any_io_executor& ui_executor);
    ~ChannelsWindow();
    void showWindow();
    bool shouldQuit() const
    {
        return quit;
    }

private:
    void loadLocalChannels();
    void showLocalChannelsTab();
    void showRemoteChannelsTab();
    void showMenu();

private:
    const boost::asio::any_io_executor& ui_executor;
    boost::asio::thread_pool remote_loading_pool;
    float bgAlpha = 0.f;
    bool quit = false;
};