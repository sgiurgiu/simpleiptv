#pragma once

#include "channels_window.h"
#include "epg_listings_window.h"
#include "playerbar_window.h"
#include "simpleiptv_vulkan.h"
#include "workers_provider.h"

#include <imgui_internal.h>
#include <boost/asio/any_io_executor.hpp>

class SimpleIPTVUI
{
public:
    SimpleIPTVUI(boost::asio::any_io_executor ui_executor,
               WorkersProvider* workersProvider,
               SimpleIPTVVulkan* vulkanInstance);
    void setSize(int width, int height);
    ImRect RenderDesktop();
    bool ShouldQuit() const
    {
        return channelsWindow->ShouldQuit() || quit;
    }
    void SetQuit(bool quit)
    {
        this->quit = quit;
    }
    template <typename S>
    void AddChannelActivatedListener(S slot)
    {
        channelActivatedSignal.connect(slot);
    }
    void ActivateNextChannel()
    {
        channelsWindow->ActivateNextChannel();
    }
    void ActivatePreviousChannel()
    {
        channelsWindow->ActivatePreviousChannel();
    }
    void SetFileLoadingError(const std::string& error)
    {
        playerBarWindow->SetFileLoadingError(error);
    }
    void SetVolume(double vol)
    {
        playerBarWindow->SetVolume(vol);
    }
    void SetAvailableSubIds(std::vector<std::string> subsIds)
    {
        playerBarWindow->SetAvailableSubIds(std::move(subsIds));
    }
    void SetPlayerState(PlayerState state)
    {
        playerBarWindow->SetCurrentPlayerState(state);
    }
    template <typename S>
    void AddPlayChannelListener(S slot)
    {
        playerBarWindow->AddPlayChannelListener(slot);
    }
    template <typename S>
    void AddStopChannelListener(S slot)
    {
        playerBarWindow->AddStopChannelListener(slot);
    }
    template <typename S>
    void AddVolumeListener(S slot)
    {
        playerBarWindow->AddVolumeListener(slot);
    }
    template <typename S>
    void AddCCButtonChangedListener(S slot)
    {
        playerBarWindow->AddCCButtonChangedListener(slot);
    }
    template <typename S>
    void AddVolumeIncreaseListener(S slot)
    {
        volumeIncreaseSignal.connect(slot);
    }
    template <typename S>
    void AddVolumeDecreaseListener(S slot)
    {
        volumeDecreaseSignal.connect(slot);
    }
    template <typename S>
    void AddVolumeToggleMuteListener(S slot)
    {
        volumeToggleMuteSignal.connect(slot);
    }
    template <typename S>
    void AddScreenshotListener(S slot)
    {
        screenshotSignal.connect(slot);
    }
    template <typename S>
    void AddScreenshotSettingsChangedListener(S slot)
    {
        screenshotSettingsChangedSignal.connect(slot);
    }
    template <typename S>
    void AddGetPlayerListener(S slot)
    {
        getPlayerSignal.connect(slot);
    }

private:
    void channelActivated(ChannelPtr channel);
    ImRect showDesktop();
private:
    WorkersProvider* workersProvider;
    std::shared_ptr<ChannelsWindow> channelsWindow;
    std::shared_ptr<PlayerBarWindow> playerBarWindow;
    std::shared_ptr<EpgListingWindow> epgListingWindow;
    using ChannelActivatedSignal = boost::signals2::signal<void(ChannelPtr)>;
    ChannelActivatedSignal channelActivatedSignal;
    using BasicOperationSignal = boost::signals2::signal<void()>;
    BasicOperationSignal volumeIncreaseSignal;
    BasicOperationSignal volumeDecreaseSignal;
    BasicOperationSignal volumeToggleMuteSignal;
    BasicOperationSignal screenshotSignal;
    BasicOperationSignal screenshotSettingsChangedSignal;
    using GetPlayerSignal = boost::signals2::signal<MpvPlayer*()>;
    GetPlayerSignal getPlayerSignal;
    bool quit = false;
    bool showChannels = true;
#ifdef STV_DEBUG
    bool showDemoWindow = true;
#endif
};