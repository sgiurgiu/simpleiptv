#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <imgui.h>
#include <memory>
#include <optional>

#include <libplacebo/colorspace.h>

#include "display_tree_nodes/display_root_channel_group.h"
#include "display_tree_nodes/display_server_node.h"

#include "aboutwindow.h"
#include "serverpopup.h"
#include "servers/server.h"
#include "settings/channel_dialog.h"
#include "settings/colorspace_dialog.h"
#include "settings/http_proxy_dialog.h"
#include "settings/screenshot_dialog.h"
#include "settings/server_dialog.h"
#include "workers_provider.h"

class ChannelsWindow : public std::enable_shared_from_this<ChannelsWindow>
{
private:
    struct Key
    {
        explicit Key() = default;
    };
    using GetPlayerSignalType = boost::signals2::signal<MpvPlayer*()>;

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
    // Returns whether the channel was found in the local channels tree and
    // activated; channels of a server subtree are not found.
    bool ActivateChannelOfGroup(ChannelsGroupPtr group, ChannelPtr channel);
    template <typename S>
    void AddGetPlayerListener(S slot)
    {
        getPlayerSignal.connect(slot);
    }
    GetPlayerSignalType& GetPlayerSignal()
    {
        return getPlayerSignal;
    }

private:
    void initialize();
    void setActivatedChannel(std::shared_ptr<DisplayChannel> channel);
    void loadLocalChannels();
    void loadSavedServers();
    void showLocalChannelsTab();
    void showRemoteChannelsTab();
    void showMenu();
    void loadServerXmlTv(ServerPtr server);

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    SimpleIPTVVulkan* vulkanInstance;
    float bgAlpha = 0.f;
    bool quit = false;
    // Declared before the trees they point into: members are destroyed in
    // reverse order, so the sets outlive the nodes that deregister from them in
    // ~DisplayNode.
    SelectionSet localSelectedNodes;
    SelectionSet remoteSelectedNodes;
    std::shared_ptr<DisplayRootChannelsGroup> rootNode;
    std::vector<std::shared_ptr<DisplayServer>> servers;
    std::string channelsFilter;
    using ChannelActivatedSignal = boost::signals2::signal<void(ChannelPtr)>;
    ChannelActivatedSignal channelActivatedSignal;
    using ScreenshotSettingsChangedSignal = boost::signals2::signal<void()>;
    ScreenshotSettingsChangedSignal screenshotSettingsChangedSignal;
    using TakeScreenshotSignal = boost::signals2::signal<void()>;
    TakeScreenshotSignal takeScreenshotSignal;
    // Weak, not raw: the activated channel is destroyed out from under us by a
    // channels reload, a "Remove Favourite", or a server refresh.
    std::weak_ptr<DisplayChannel> activatedChannel;
    bool pinned = false;
    ColorspaceDialog colorspaceDialog;
    std::shared_ptr<HTTPProxyDialog> httpProxyDialog;
    std::shared_ptr<ScreenshotDialog> screenshotDialog;
    std::shared_ptr<ServerDialog> serverDialog;
    std::shared_ptr<ChannelDialog> channelDialog;
    std::optional<int> width;
    AboutWindow aboutWindow;

    GetPlayerSignalType getPlayerSignal;
};
