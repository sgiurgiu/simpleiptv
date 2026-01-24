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
#include <mutex>
#include <thread>

struct mpv_handle;
struct mpv_render_context;
struct mpv_event;

class SimpleIPTV;

class MpvPlayer
{
public:
    MpvPlayer(boost::asio::any_io_executor ui_executor,
              WorkersProvider* workersProvider,
              SimpleIPTVVulkan* vulkanInstance);
    ~MpvPlayer();
    void InitializeMpv(SimpleIPTV* iptv);
    void Render();
    void ReportSwap();
    void SetSize(int width, int height);
    void Play(ChannelPtr channel);
    void Play();
    void Stop();
    void Pause();
    PlayerState GetPlayerState() const;
    void VolumeToggleMute();
    void VolumeIncrease();
    void VolumeDecrease();
    double GetVolume() const;
    void SetVolume(double volume);

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

private:
    void handleMpvEvent(mpv_event* event);
    void handleMpvEvents();
    bool mpvRenderFrame(pl_swapchain_frame* frame, const ImRect& desktopRect);
    static void mpvRenderUpdate(void* ctx);
    static void onMpvEvents(void* ctx);

    void removeVolumeOsd(const boost::system::error_code& ec);

    void proxySettings(HttpProxy proxy);

    void mpvRenderThread();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    boost::asio::steady_timer osdTimer;
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvRenderContext = nullptr;
    pl_options placeboOptions = nullptr;
    double volume = 100.0;

    std::atomic_int width = 100;
    std::atomic_int height = 100;

    PlayerState playerState = PlayerState::STOPPED;
    ChannelPtr currentlyPlayingChannel;
    int skipRendering = 0;
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
    SimpleIPTV* iptv = nullptr;
};
