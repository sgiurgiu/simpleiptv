#include <libplacebo/renderer.h>
#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include "mpvplayer.h"

#include <GLFW/glfw3.h>
#include <boost/asio/post.hpp>
#include <mpv/client.h>
#include <mpv/render_placebo.h>

#include <stdexcept>

#ifdef STV_UNIX
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif

#include "mpvhelper.h"
#include <fmt/format.h>
#include <functional>
#include <spdlog/spdlog.h>

namespace
{

const std::string frameVertexShaderText = R"*(
#version 330
in vec4 position;
in vec4 inputTextureCoordinate;
out vec2 textureCoordinate;
void main()
{
    gl_Position = position;
    textureCoordinate = inputTextureCoordinate.xy;
}
)*";
const std::string frameFragmentShaderText = R"*(
#version 330
in vec2 textureCoordinate;
uniform sampler2D videoFrame;
out vec4 color;
void main()
{
    color = texture2D(videoFrame, textureCoordinate);
}
)*";

constexpr int VOLUME_OSD_ID = 1;
constexpr std::chrono::duration OSD_DURATION = std::chrono::seconds{ 3 };

} // namespace

MpvPlayer::MpvPlayer(const boost::asio::any_io_executor &ui_executor,
                     WorkersProvider *workersProvider)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, osdTimer{ this->workersProvider->GetNetworkExecutor() }
{
    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    mpv_set_property_string(mpv, "terminal", "yes");
    mpv_set_property_string(mpv, "msg-level", "all=info");
    mpv_set_property_string(mpv, "sub-create-cc-track", "yes");
    mpv_set_property_string(mpv, "input-default-bindings", "no");
    mpv_set_property_string(mpv, "config", "no");
    mpv_set_property_string(mpv, "input-vo-keyboard", "no");
    mpv_set_property_string(mpv, "vo", "libmpv");

#ifdef STV_UNIX
    // mpv_set_property_string(mpv, "ao", "auto");
#else
    // mpv_set_property_string(mpv, "ao", "auto");
#endif
    mpv_set_property_string(mpv, "hwdec", "no");
    mpv_observe_property(mpv, 0, "height", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "width", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    int64_t cacheSecs = 30;
    mpv_set_property(mpv, "cache-secs", MPV_FORMAT_INT64, &cacheSecs);
    mpv_set_property(mpv, "demuxer-readahead-secs", MPV_FORMAT_INT64, &cacheSecs);
    using namespace std::placeholders;
    auto proxySettingsCb = std::bind(&MpvPlayer::proxySettings, this, _1);
    this->workersProvider->GetProxyRepository()->LoadConfiguredProxy(
        proxySettingsCb, ui_executor);
    this->workersProvider->GetProxyRepository()->AddUpdatedProxySignalListener(
        proxySettingsCb);

    mpv_set_option_string(mpv, "ytdl", "no");
    // mpv_set_option_string(mpv, "gpu-debug", "true");

    double volMax = 150.0;
    mpv_set_property(mpv, "volume-max", MPV_FORMAT_DOUBLE, &volMax);

    mpv_set_wakeup_callback(mpv, MpvPlayer::onMpvEvents, this);

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");
    int errorCode = mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
    if (errorCode)
    {
        spdlog::error("Cannot set volume: {}", mpv_error_string(errorCode));
    }
}

void MpvPlayer::proxySettings(HttpProxy proxy)
{
    if (proxy.use)
    {
        auto proxyUrl = fmt::format("http://{}:{}", proxy.host, proxy.port);
        mpv_set_property_string(mpv, "http-proxy", proxyUrl.c_str());
    }
    else
    {
        mpv_set_property_string(mpv, "http-proxy", "");
    }
}

MpvPlayer::~MpvPlayer()
{
    if (mpv)
    {
        const char *cmd[] = { "quit", nullptr };
        mpv_command(mpv, cmd);
    }
    if (mpvRenderContext)
    {
        mpv_render_context_free(mpvRenderContext);
        mpvRenderContext = nullptr;
    }
    if (mpv)
    {
        mpv_set_wakeup_callback(mpv, nullptr, nullptr);
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
    workersProvider->GetSleepService()->enableComputerSleep();
    if (placeboOptions)
    {
        pl_options_free(&placeboOptions);
        placeboOptions = nullptr;
    }
}

void MpvPlayer::InitializeMpv(pl_swapchain swapchain, pl_log log)
{
    placeboOptions = pl_options_alloc(log);
    placeboOptions->params = pl_render_high_quality_params;

    int mpv_advanced_control = 1;

    mpv_render_param libplacebo_params[] = {
        { MPV_RENDER_PARAM_API_TYPE,
          const_cast<char *>(MPV_RENDER_API_TYPE_LIBPLACEBO) },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_PL_LOG,
          (void *)log },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN,
          (void *)swapchain },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &mpv_advanced_control },
        { MPV_RENDER_PARAM_INVALID, 0 }
    };

    mpv_render_context_create(&mpvRenderContext, mpv, libplacebo_params);

    mpv_render_context_set_update_callback(mpvRenderContext,
                                           &MpvPlayer::mpvRenderUpdate, this);
}

void MpvPlayer::onMpvEvents(void *ctx)
{
    auto self = reinterpret_cast<MpvPlayer *>(ctx);
    boost::asio::post(self->ui_executor,
                      std::bind(&MpvPlayer::handleMpvEvents, self));
}
void MpvPlayer::handleMpvEvents()
{
    while (mpv)
    {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
        {
            break;
        }
        handleMpvEvent(event);
    }
}
void MpvPlayer::handleMpvEvent(mpv_event *event)
{
    switch (event->event_id)
    {
    case MPV_EVENT_PROPERTY_CHANGE:
    {
        mpv_event_property *prop = (mpv_event_property *)event->data;
        std::string name(prop->name);
        if (prop->format == MPV_FORMAT_DOUBLE)
        {
            double value = *(double *)prop->data;
            if (name == "volume")
            {
                mediaState.volume = value;
            }
        }
        else if (prop->format == MPV_FORMAT_FLAG)
        {
            int value = *(int *)prop->data;
            if (name == "paused")
            {
                mediaState.paused = (bool)value;
                playerState = PlayerState::PAUSED;
                playerStateSignal(playerState);
            }
        }
        break;
    }
    case MPV_EVENT_VIDEO_RECONFIG:
    {
        double propValue;
        mpv_get_property(mpv, "width", mpv_format::MPV_FORMAT_DOUBLE, &propValue);
        if (mediaState.width != propValue)
        {
            mediaState.width = propValue;
        }
        mpv_get_property(mpv, "height", mpv_format::MPV_FORMAT_DOUBLE,
                         &propValue);
        if (mediaState.height != propValue)
        {
            mediaState.height = propValue;
        }
        break;
    }
    case MPV_EVENT_END_FILE:
    {
        mpv_event_end_file *end_file =
            static_cast<mpv_event_end_file *>(event->data);
        switch (end_file->reason)
        {
        case mpv_end_file_reason::MPV_END_FILE_REASON_ERROR:
            playerState = PlayerState::LOADING_ERROR;
            playerStateSignal(playerState);
            fileLoadingErrorSignal(mpv_error_string(end_file->error));
            break;
        case mpv_end_file_reason::MPV_END_FILE_REASON_EOF:
            [[fallthrough]];
        case mpv_end_file_reason::MPV_END_FILE_REASON_STOP:
            playerState = PlayerState::STOPPED;
            playerStateSignal(playerState);
            break;
        default:
            break;
        }
        workersProvider->GetSleepService()->enableComputerSleep();
    }
    break;
    case MPV_EVENT_FILE_LOADED:
    {
        playerState = PlayerState::PLAYING;
        workersProvider->GetSleepService()->disableComputerSleep();
        skipRendering = 0;
        playerStateSignal(playerState);
        int tracksCount = 0;
        mpv_get_property(mpv, "track-list/count", MPV_FORMAT_INT64, &tracksCount);
        std::vector<std::string> subIds;
        for (int i = 0; i < tracksCount; i++)
        {
            char *type = mpv_get_property_string(
                mpv, fmt::format("track-list/{}/type", i).c_str());
            if (type == std::string("sub"))
            {
                char *id = mpv_get_property_string(
                    mpv, fmt::format("track-list/{}/id", i).c_str());
                subIds.emplace_back(id);
            }
        }
        subsAvailableSignal(std::move(subIds));
    }
    break;
    default:
        break;
        // Ignore uninteresting or unknown events.
    }
}

void MpvPlayer::mpvRenderUpdate(void *ctx)
{
    auto self = reinterpret_cast<MpvPlayer *>(ctx);
    self->shouldRender = true;
    // boost::asio::post(self->ui_executor,
    //                   std::bind(&MpvPlayer::updateDisplay, self));
}

void MpvPlayer::updateDisplay()
{
    // mpvRenderFrame();
}

void MpvPlayer::mpvRenderFrame(pl_swapchain_frame *frame)
{
    if (!shouldRender)
        return;
    shouldRender = false;
    int block = 0;
    mpv_render_param render_params[] = {
        { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_OPTIONS,
          (void *)placeboOptions },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_FRAME,
          (void *)frame },
        { MPV_RENDER_PARAM_INVALID, 0 }
    };
    int flip_y = 0;
    uint64_t flags = mpv_render_context_update(mpvRenderContext);

    if (flags & MPV_RENDER_UPDATE_FRAME)
    {
        mpv_render_context_render(mpvRenderContext, render_params);
    }
}

void MpvPlayer::Render(pl_swapchain_frame *frame, const ImVec2 &windowsSize)
{
    mpvRenderFrame(frame);
}
void MpvPlayer::SetSize(int width, int height)
{
    if (width == this->width && height == this->height)
        return;
    this->width = width;
    this->height = height;
    frameWidth = width - lastWindowSize.x;
    frameHeight = height - lastWindowSize.y;
}
void MpvPlayer::Play()
{
    if (!currentlyPlayingChannel || playerState == PlayerState::PLAYING)
        return;
    Play(currentlyPlayingChannel);
}
void MpvPlayer::Stop()
{
    const char *cmdStop[] = { "stop", nullptr };
    mpv_command(mpv, cmdStop);
    playerState = PlayerState::STOPPED;
    // playerStateSignal(playerState);
}
void MpvPlayer::Pause()
{
    if (playerState != PlayerState::PLAYING)
        return;
    int pause = 1;
    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    playerState = PlayerState::PAUSED;
    playerStateSignal(playerState);
}

void MpvPlayer::Play(ChannelPtr channel)
{
    if (playerState == PlayerState::PLAYING && channel == currentlyPlayingChannel)
        return;
    const char *cmdStop[] = { "stop", nullptr };
    mpv_command(mpv, cmdStop);

    this->currentlyPlayingChannel = channel;
    skipRendering = 1;
    const char *cmd[] = { "loadfile", currentlyPlayingChannel->GetUri().c_str(),
                          nullptr };
    mpv_command(mpv, cmd);
    mpv_set_property_string(mpv, "pause", "no");
    mpv_set_property_string(mpv, "sid", "no");
    mpv_set_property_string(mpv, "loop-playlist", "inf");
}

PlayerState MpvPlayer::GetPlayerState() const
{
    return playerState;
}
void MpvPlayer::VolumeToggleMute()
{
    int mute = 0;
    mpv_get_property(mpv, "mute", MPV_FORMAT_FLAG, &mute);
    mute = mute ? 0 : 1;
    mpv_set_property(mpv, "mute", MPV_FORMAT_FLAG, &mute);
    spdlog::debug("mute set to {}", mute);
    NodeVariant node = NodeVariantMap{
        { "name", { "osd-overlay" } },
        { "id", { 1 } },
        { "format", { "ass-events" } },
        { "res_x", { width } },
        { "res_y", { height } },
        { "data", { fmt::format("{{\\an9\\fs36}}Mute {}", mute ? "on" : "off") } }
    };
    NodeBuilder builder{ node };
    mpv_command_node(mpv, builder.GetNode(), nullptr);
    using namespace std::placeholders;
    osdTimer.expires_after(OSD_DURATION);
    osdTimer.async_wait(std::bind(&MpvPlayer::removeVolumeOsd, this, _1));
    volumeSignal(mute ? 0.0 : volume);
}
void MpvPlayer::VolumeIncrease()
{
    volume += 5.0;
    SetVolume(volume);
}
void MpvPlayer::VolumeDecrease()
{
    volume -= 5.0;
    if (volume < 0.0)
        volume = 0.0;
    SetVolume(volume);
}
void MpvPlayer::removeVolumeOsd(const boost::system::error_code &ec)
{
    if (!ec)
    {
        NodeVariant node = NodeVariantMap{ { "name", { "osd-overlay" } },
                                           { "id", { VOLUME_OSD_ID } },
                                           { "format", { "none" } },
                                           { "data", { "" } } };
        NodeBuilder builder{ node };
        mpv_command_node(mpv, builder.GetNode(), nullptr);
    }
}
double MpvPlayer::GetVolume() const
{
    return volume;
}
void MpvPlayer::SetVolume(double volume)
{
    this->volume = volume;
    mpv_set_property_string(mpv, "mute", "no");

    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &this->volume);
    spdlog::debug("volume set to {}", this->volume);

    NodeVariant node = NodeVariantMap{
        { "name", { "osd-overlay" } },
        { "id", { VOLUME_OSD_ID } },
        { "format", { "ass-events" } },
        { "res_x", { width } },
        { "res_y", { height } },
        { "data", { fmt::format("{{\\an9\\fs36}}Volume {}", (int)this->volume) } }
    };
    NodeBuilder builder{ node };
    mpv_command_node(mpv, builder.GetNode(), nullptr);
    using namespace std::placeholders;
    osdTimer.expires_after(OSD_DURATION);
    osdTimer.async_wait(std::bind(&MpvPlayer::removeVolumeOsd, this, _1));
    volumeSignal(this->volume);
}
void MpvPlayer::ClosedCaptions(const std::string &id)
{
    mpv_set_property_string(mpv, "sid", id.c_str());
}