#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <memory>

#include "channels/root_channel_group.h"
#include "serverpopup.h"
#include "workers_provider.h"

class ChannelsWindow : public std::enable_shared_from_this<ChannelsWindow>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ChannelsWindow(Key,
                   const boost::asio::any_io_executor& ui_executor,
                   WorkersProvider& workersProvider);
    static std::shared_ptr<ChannelsWindow>
    Create(const boost::asio::any_io_executor& executor,
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
    boost::asio::any_io_executor ui_executor;
    WorkersProvider& workersProvider;
    float bgAlpha = 0.f;
    bool quit = false;
    RootChannelsGroupPtr root;
};
