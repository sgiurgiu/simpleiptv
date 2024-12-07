#pragma once

#include <boost/asio/any_io_executor.hpp>

#include "channels/root_channel_group.h"
#include "serverpopup.h"
#include "workers_provider.h"

class ChannelsWindow
{
public:
    ChannelsWindow(const boost::asio::any_io_executor& ui_executor,
                   WorkersProvider& workersProvider);
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
    WorkersProvider& workersProvider;
    float bgAlpha = 0.f;
    bool quit = false;
    RootChannelsGroupPtr root;
};
