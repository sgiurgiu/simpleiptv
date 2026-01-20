#pragma once

#include "channels/channel.h"
#include "mpvplayer_state.h"
#include "proxy_repository.h"
#include "workers_provider.h"
#include <atomic>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>
#include <imgui_internal.h>
#include <libplacebo/log.h>
#include <libplacebo/options.h>
#include <libplacebo/swapchain.h>

struct mpv_handle;
struct mpv_render_context;
struct mpv_event;

class MpvPlayer
{
public:
    MpvPlayer(const boost::asio::any_io_executor& ui_executor,
              WorkersProvider* workersProvider);
    ~MpvPlayer();
    void InitializeMpv(pl_swapchain swapchain, pl_log logger);
    void Render(pl_swapchain_frame* frame, const ImRect& desktopRect);
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
    void mpvRenderFrame(pl_swapchain_frame* frame, const ImRect& desktopRect);
    static void mpvRenderUpdate(void* ctx);
    static void onMpvEvents(void* ctx);

    void updateDisplay();
    void removeVolumeOsd(const boost::system::error_code& ec);

    void proxySettings(HttpProxy proxy);

private:
    const boost::asio::any_io_executor& ui_executor;
    WorkersProvider* workersProvider;
    boost::asio::steady_timer osdTimer;
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvRenderContext = nullptr;
    pl_options placeboOptions = nullptr;
    double volume = 100.0;
    struct MediaState
    {
        double width = 0.0;
        double height = 0.0;
        double volume = 0.0;
        bool paused = true;
    };
    MediaState mediaState;
    int width = 100;
    int height = 100;
    int frameWidth = width;
    int frameHeight = width;
    ImVec2 lastWindowSize = { 0, 0 };

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
    std::atomic_bool shouldRender = false;
};
