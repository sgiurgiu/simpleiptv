#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <imgui.h>
#include <memory>

#include "channels/root_channel_group.h"
#include "display_tree_node.h"
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
                   WorkersProvider* workersProvider);
    static std::shared_ptr<ChannelsWindow>
    Create(const boost::asio::any_io_executor& executor,
           WorkersProvider* workersProvider);

    ImVec2 ShowWindow(float playerBarHeight);
    bool ShouldQuit() const
    {
        return quit;
    }
    bool IsPinned() const
    {
        return pinned;
    }
    template <typename S>
    void AddChannelActivatedListener(S slot)
    {
        channelActivatedSignal.connect(slot);
    }
    void ActivateNextChannel();
    void ActivatePreviousChannel();
    void ActivateChannelOfGroup(ChannelsGroupPtr group, ChannelPtr channel);

private:
    void initialize();
    void loadLocalChannels();
    void loadSavedServers();
    void showLocalChannelsTab();
    void showRemoteChannelsTab();
    void showMenu();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    float bgAlpha = 0.f;
    bool quit = false;
    std::shared_ptr<DisplayRootChannelsGroup> rootNode;
    std::vector<std::shared_ptr<DisplayServer>> servers;
    std::string channelsFilter;
    using ChannelActivatedSignal = boost::signals2::signal<void(ChannelPtr)>;
    ChannelActivatedSignal channelActivatedSignal;
    DisplayChannel* activatedChannel = nullptr;
    bool pinned = false;
};
