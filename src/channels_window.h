#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <imgui.h>
#include <memory>
#include <optional>

#include "display_tree_nodes/display_root_channel_group.h"
#include "display_tree_nodes/display_server_node.h"

#include "serverpopup.h"
#include "settings/http_proxy_dialog.h"
#include "settings/screenshot_dialog.h"
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
                   WorkersProvider* workersProvider,
                   SimpleIPTVVulkan* vulkanInstance);
    static std::shared_ptr<ChannelsWindow>
    Create(const boost::asio::any_io_executor& executor,
           WorkersProvider* workersProvider,
           SimpleIPTVVulkan* vulkanInstance);

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
    template <typename S>
    void AddScreenshotSettingsChangedListener(S slot)
    {
        screenshotSettingsChangedSignal.connect(slot);
    }
    template <typename S>
    void AddTakeScreenshotListener(S slot)
    {
        takeScreenshotSignal.connect(slot);
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
    SimpleIPTVVulkan* vulkanInstance;
    float bgAlpha = 0.f;
    bool quit = false;
    std::shared_ptr<DisplayRootChannelsGroup> rootNode;
    std::vector<std::shared_ptr<DisplayServer>> servers;
    std::string channelsFilter;
    using ChannelActivatedSignal = boost::signals2::signal<void(ChannelPtr)>;
    ChannelActivatedSignal channelActivatedSignal;
    using ScreenshotSettingsChangedSignal = boost::signals2::signal<void()>;
    ScreenshotSettingsChangedSignal screenshotSettingsChangedSignal;
    using TakeScreenshotSignal = boost::signals2::signal<void()>;
    TakeScreenshotSignal takeScreenshotSignal;
    DisplayChannel* activatedChannel = nullptr;
    bool pinned = false;
    std::shared_ptr<HTTPProxyDialog> httpProxyDialog;
    std::shared_ptr<ScreenshotDialog> screenshotDialog;
    std::optional<int> width;
};
