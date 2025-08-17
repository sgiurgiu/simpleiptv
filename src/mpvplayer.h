#pragma once

#include <GL/gl.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>
#include <imgui.h>

#include "channels/channel.h"
#include "mpvplayer_state.h"
#include "proxy_repository.h"
#include "workers_provider.h"

struct mpv_handle;
struct mpv_render_context;
struct mpv_event;

class MpvPlayer
{
public:
    MpvPlayer(const boost::asio::any_io_executor& ui_executor,
              WorkersProvider* workersProvider);
    ~MpvPlayer();
    void InitializeMpvGL();
    void Render(const ImVec2& windowsSize);
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
    void mpvRenderFrame();
    void createFrameBuffers();
    void destroyFrameBuffers();
    void rescaleFrameBuffers();
    static void mpvRenderUpdate(void* ctx);
    static void onMpvEvents(void* ctx);
    void compileShaders();
    void initializeVAO();

    void updateDisplay();
    void removeVolumeOsd(const boost::system::error_code& ec);

    void proxySettings(HttpProxy proxy);

private:
    const boost::asio::any_io_executor& ui_executor;
    WorkersProvider* workersProvider;
    boost::asio::steady_timer osdTimer;
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvRenderContext = nullptr;
    double volume = 100.0;
    struct MediaState
    {
        double width = 0.0;
        double height = 0.0;
        double volume = 0.0;
        bool paused = true;
    };
    MediaState mediaState;
    GLuint mediaFramebufferObject = 0;
    GLuint mediaFrameTexture = 0;
    // GLuint mediaFrameRenderBufferObject = 0;
    int width = 100;
    int height = 100;
    int frameWidth = width;
    int frameHeight = width;
    ImVec2 lastWindowSize = { 0, 0 };
    GLuint frameShaderProgram;
    GLint videoFrameUniformLocation;

    GLint shaderPositionAttribLocation;
    GLint shaderTextCoordinateLocation;

    GLuint VAO;
    GLuint buffs[2];

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
};
