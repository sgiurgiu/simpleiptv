#pragma once

#include "channels/channel.h"
#include "mpvplayer_state.h"
#include "proxy_repository.h"
#include "simpleiptv_vulkan.h"
#include "workers_provider.h"

#include <atomic>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>
#include <imgui_internal.h>
#include <libplacebo/log.h>
#include <libplacebo/options.h>
#include <libplacebo/swapchain.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

struct mpv_handle;
struct mpv_render_context;
struct mpv_event;

class MpvPlayer
{
public:
    MpvPlayer(boost::asio::any_io_executor ui_executor,
              WorkersProvider* workersProvider,
              SimpleIPTVVulkan* vulkanInstance);
    ~MpvPlayer();
    using UIRenderCallback = std::function<ImRect()>;
    void InitializeMpv(UIRenderCallback uiRenderCallback);
    void Render();
    void SetSize(int width, int height);
    void Play(ChannelPtr channel);
    void Play();
    void Stop();
    void Pause();
    void Screenshot();
    void SetScreenshotPath(const std::filesystem::path& path);
    void SetScreenshotFileTemplate(const std::string& fileTemplate);
    void SetScreenshotFormat(const std::string& format);

    PlayerState GetPlayerState() const;
    void VolumeToggleMute();
    void VolumeIncrease();
    void VolumeDecrease();
    double GetVolume() const;
    void SetVolume(double volume);
    void SetColorspace(const pl_color_space& colorspace);
    pl_color_space GetColorspace() const;

    template <typename S>
    void AddPlayerStateListener(S slot)
    {
        playerStateSignal.connect(slot);
    }
    template <typename S>
    void AddFileLoadingErrorListener(S slot)
    {
        fileLoadingErrorSignal.connect(slot);
    }
    template <typename S>
    void AddVolumeListener(S slot)
    {
        volumeSignal.connect(slot);
    }
    template <typename S>
    void AddSubsAvailableListener(S slot)
    {
        subsAvailableSignal.connect(slot);
    }
    void ClosedCaptions(const std::string& id);
    pl_color_space GetDefaultColorspace() const;

private:
    void handleMpvEvent(mpv_event* event);
    void handleMpvEvents();
    bool mpvRenderFrame(pl_swapchain_frame* frame, const ImRect& desktopRect);
    static void mpvRenderUpdate(void* ctx);
    static void onMpvEvents(void* ctx);

    void removeVolumeOsd(const boost::system::error_code& ec);
    void removeScreenshotOsd(const boost::system::error_code& ec);
    void showScreenshotOsd(const std::string& filename, bool error = false);
    void proxySettings(HttpProxy proxy);

    void mpvRenderThread();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    boost::asio::steady_timer osdVolumeTimer;
    boost::asio::steady_timer osdScreenshotTimer;
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvRenderContext = nullptr;
    pl_options placeboOptions = nullptr;
    double volume = 100.0;
    mutable std::mutex colorspaceMutex;
    pl_color_space colorspace;

    std::atomic_int width = 100;
    std::atomic_int height = 100;

    std::atomic<PlayerState> playerState = PlayerState::STOPPED;
    mutable std::mutex currentlyPlayingChannelMutex;
    ChannelPtr currentlyPlayingChannel;
    std::atomic_uint64_t pendingLoadRequestId;
    std::atomic_bool stopRequested = false;
    using PlayerStateSignal = boost::signals2::signal<void(PlayerState)>;
    using FileLoadingErrorSignal =
        boost::signals2::signal<void(const std::string&)>;
    using VolumeSignal = boost::signals2::signal<void(double)>;
    PlayerStateSignal playerStateSignal;
    FileLoadingErrorSignal fileLoadingErrorSignal;
    VolumeSignal volumeSignal;
    using SubsAvailableSignal =
        boost::signals2::signal<void(std::vector<std::string>)>;
    SubsAvailableSignal subsAvailableSignal;

    bool renderInProgress = false;
    std::atomic_bool needsResize = false;
    std::mutex renderWakeupMutex;
    std::condition_variable renderWakeupCondition;
    std::atomic_bool renderThreadQuit = false;
    std::atomic_bool shouldRender = false;

    SimpleIPTVVulkan* vulkanInstance = nullptr;

    std::thread renderThread;
    std::thread mpvEventsThread;
    UIRenderCallback uiRenderCallback;
};
