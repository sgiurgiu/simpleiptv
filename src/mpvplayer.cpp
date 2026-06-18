
#include "mpvplayer.h"
#include "mpvhelper.h"

#include <libplacebo/colorspace.h>
#include <libplacebo/renderer.h>
#include <libplacebo/swapchain.h>

#include <boost/asio/post.hpp>
#include <mpv/client.h>
#include <mpv/render_placebo.h>

#include <chrono>
#include <stdexcept>

#include <fmt/format.h>
#include <functional>
#include <spdlog/spdlog.h>

namespace
{

constexpr int VOLUME_OSD_ID = 1;
constexpr int SCREENSHOT_OSD_ID = 2;
constexpr std::chrono::duration OSD_DURATION = std::chrono::seconds{ 3 };

constexpr uint64_t STOP_COMMAND_REPLY_USERDATA = 15;
constexpr uint64_t SCREENSHOT_COMMAND_REPLY_USERDATA = 17;
constexpr uint64_t LOADFILE_COMMAND_REPLY_USERDATA_BASE = 1000;

} // namespace

MpvPlayer::MpvPlayer(boost::asio::any_io_executor ui_executor,
                     WorkersProvider *workersProvider,
                     SimpleIPTVVulkan *vulkanInstance)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, osdVolumeTimer{ this->workersProvider->GetNetworkExecutor() }
, osdScreenshotTimer{ this->workersProvider->GetNetworkExecutor() }
, pendingLoadRequestId{ LOADFILE_COMMAND_REPLY_USERDATA_BASE }
, vulkanInstance{ vulkanInstance }
{
    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    mpv_set_property_string(mpv, "terminal", "yes");
#ifdef STV_DEBUG
    mpv_set_property_string(mpv, "msg-level", "all=v");
#else
    mpv_set_property_string(mpv, "msg-level", "all=error");
#endif
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
    mpv_set_property_string(mpv, "hwdec", "auto");
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

    SetScreenshotPath(
        this->workersProvider->GetSettingsRepository()->GetScreenshotPath(
            std::filesystem::path(".")));
    SetScreenshotFormat(
        this->workersProvider->GetSettingsRepository()->GetScreenshotFormat(
            "jpg"));
    SetScreenshotFileTemplate(
        this->workersProvider->GetSettingsRepository()->GetScreenshotFileTemplate(
            "screenshot_%04n"));
    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");
    int errorCode = mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
    if (errorCode)
    {
        spdlog::error("Cannot set volume: {}", mpv_error_string(errorCode));
    }
    mpvEventsThread = std::thread([this]() { handleMpvEvents(); });
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
    renderThreadQuit = true;
    renderWakeupCondition.notify_all();
    if (renderThread.joinable())
    {
        renderThread.join();
    }
    if (mpv)
    {
        const char *cmd[] = { "quit", nullptr };
        mpv_command(mpv, cmd);
    }
    if (mpvEventsThread.joinable())
    {
        mpvEventsThread.join();
    }
    osdVolumeTimer.cancel();
    osdScreenshotTimer.cancel();
    if (mpvRenderContext)
    {
        mpv_render_context_free(mpvRenderContext);
        mpvRenderContext = nullptr;
    }
    if (mpv)
    {
        // mpv_set_wakeup_callback(mpv, nullptr, nullptr);
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

void MpvPlayer::InitializeMpv(UIRenderCallback uiRenderCallback)
{
    this->uiRenderCallback = std::move(uiRenderCallback);
    placeboOptions = pl_options_alloc(vulkanInstance->GetPlLog());
    placeboOptions->params = pl_render_high_quality_params;

    int mpv_advanced_control = 1;

    mpv_render_param libplacebo_params[] = {
        { MPV_RENDER_PARAM_API_TYPE,
          const_cast<char *>(MPV_RENDER_API_TYPE_LIBPLACEBO) },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_PL_LOG,
          (void *)vulkanInstance->GetPlLog() },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN,
          (void *)vulkanInstance->GetPlSwapchain() },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &mpv_advanced_control },
        { MPV_RENDER_PARAM_INVALID, 0 }
    };

    int errorCode =
        mpv_render_context_create(&mpvRenderContext, mpv, libplacebo_params);
    if (errorCode)
    {
        spdlog::error("Cannot create mpv render context: {}",
                      mpv_error_string(errorCode));
        throw std::runtime_error(fmt::format(
            "Cannot create mpv render context: {}", mpv_error_string(errorCode)));
    }

    mpv_render_context_set_update_callback(mpvRenderContext,
                                           &MpvPlayer::mpvRenderUpdate, this);
    renderThread = std::thread([this]() { mpvRenderThread(); });
}

void MpvPlayer::handleMpvEvents()
{
    while (mpv)
    {
        mpv_event *event = mpv_wait_event(mpv, -1);
        if (event->event_id == MPV_EVENT_NONE)
        {
            continue;
        }
        if (event->event_id == MPV_EVENT_SHUTDOWN)
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
            if (name == "volume")
            {
            }
        }
        else if (prop->format == MPV_FORMAT_FLAG)
        {
            int value = *(int *)prop->data;
            if (name == "paused" || name == "pause")
            {
                if (value)
                {
                    boost::asio::post(ui_executor,
                                      [this]()
                                      {
                                          playerState = PlayerState::PAUSED;
                                          playerStateSignal(playerState);
                                      });
                }
            }
        }
        break;
    }
    case MPV_EVENT_VIDEO_RECONFIG:
    {
        /*double propValue;
        mpv_get_property(mpv, "width", mpv_format::MPV_FORMAT_DOUBLE,
        &propValue); if (mediaState.width != propValue)
        {
            mediaState.width = propValue;
        }
        mpv_get_property(mpv, "height", mpv_format::MPV_FORMAT_DOUBLE,
                         &propValue);
        if (mediaState.height != propValue)
        {
            mediaState.height = propValue;
        }*/
        break;
    }
    case MPV_EVENT_END_FILE:
    {
        mpv_event_end_file *end_file =
            static_cast<mpv_event_end_file *>(event->data);
        switch (end_file->reason)
        {
        case mpv_end_file_reason::MPV_END_FILE_REASON_ERROR:
        {
            std::string error = mpv_error_string(end_file->error);
            boost::asio::post(ui_executor,
                              [this, error]()
                              {
                                  playerState = PlayerState::LOADING_ERROR;
                                  playerStateSignal(playerState);
                                  fileLoadingErrorSignal(error);
                              });
        }
        break;
        case mpv_end_file_reason::MPV_END_FILE_REASON_EOF:
            [[fallthrough]];
        case mpv_end_file_reason::MPV_END_FILE_REASON_STOP:
            boost::asio::post(ui_executor,
                              [this]()
                              {
                                  playerState = PlayerState::STOPPED;
                                  playerStateSignal(playerState);
                              });
            break;
        default:
            break;
        }
        workersProvider->GetSleepService()->enableComputerSleep();
    }
    break;
    case MPV_EVENT_FILE_LOADED:
    {
        // Query the track list here, on the mpv events thread — NOT inside the
        // ui_executor handler below. PollUI runs ui_executor handlers while
        // holding uiStateMutex, and a synchronous mpv_get_property under that
        // lock deadlocks: the render thread blocks on uiStateMutex, while mpv's
        // advanced-control core blocks waiting for the render thread.
        int64_t tracksCount = 0;
        mpv_get_property(mpv, "track-list/count", MPV_FORMAT_INT64,
                         &tracksCount);
        std::vector<std::string> subIds;
        for (int64_t i = 0; i < tracksCount; i++)
        {
            char *type = mpv_get_property_string(
                mpv, fmt::format("track-list/{}/type", i).c_str());
            if (type && type == std::string("sub"))
            {
                char *id = mpv_get_property_string(
                    mpv, fmt::format("track-list/{}/id", i).c_str());
                subIds.emplace_back(id);
                mpv_free(id);
            }
            mpv_free(type);
        }
        boost::asio::post(
            ui_executor,
            [this, subIds = std::move(subIds)]() mutable
            {
                playerState = PlayerState::PLAYING;
                workersProvider->GetSleepService()->disableComputerSleep();
                playerStateSignal(playerState);
                subsAvailableSignal(std::move(subIds));
            });
    }
    break;
    case MPV_EVENT_COMMAND_REPLY:
    {
        auto reply_userdata = event->reply_userdata;
        if (reply_userdata >= LOADFILE_COMMAND_REPLY_USERDATA_BASE)
        {
            if (reply_userdata != pendingLoadRequestId.load() ||
                stopRequested.load())
            {
                break;
            }
            ChannelPtr channel;
            {
                std::lock_guard<std::mutex> _{ currentlyPlayingChannelMutex };
                channel = currentlyPlayingChannel;
            }
            if (!channel)
            {
                spdlog::debug(
                    "Ignoring stale load reply with no active channel");
                break;
            }
            const char *cmd[] = { "loadfile", channel->GetUri().c_str(), nullptr };
            mpv_command(mpv, cmd);
            mpv_set_property_string(mpv, "pause", "no");
            mpv_set_property_string(mpv, "sid", "no");
            mpv_set_property_string(mpv, "loop-playlist", "inf");
            break;
        }
        switch (reply_userdata)
        {
        case STOP_COMMAND_REPLY_USERDATA:
            boost::asio::post(ui_executor,
                              [this]()
                              {
                                  playerState = PlayerState::STOPPED;
                                  playerStateSignal(playerState);
                              });
            break;
        case SCREENSHOT_COMMAND_REPLY_USERDATA:
        {
            std::string filename;
            if (event->error != 0)
            {
                spdlog::error("Screenshot command replied with error: {}",
                              mpv_error_string(event->error));
                filename = mpv_error_string(event->error);
            }
            else
            {
                mpv_node *node = (mpv_node *)event->data;
                if (node->format == MPV_FORMAT_NODE_MAP)
                {
                    mpv_node_list *map = node->u.list;
                    for (int i = 0; i < map->num; i++)
                    {
                        std::string key = map->keys[i];
                        std::string value = map->values[i].u.string;
                        if (key == "filename")
                        {
                            spdlog::debug("Screenshot command replied: {}: {}",
                                          key, value);
                            filename = value;
                        }
                    }
                }
            }
            if (!filename.empty())
            {
                showScreenshotOsd(filename, event->error != 0);
                using namespace std::placeholders;
                osdScreenshotTimer.expires_after(OSD_DURATION);
                osdScreenshotTimer.async_wait(
                    std::bind(&MpvPlayer::removeScreenshotOsd, this, _1));
            }
            break;
        }
        default:
            break;
        }
    }
    default:
        break;
        // Ignore uninteresting or unknown events.
    }
}

void MpvPlayer::mpvRenderUpdate(void *ctx)
{
    auto self = reinterpret_cast<MpvPlayer *>(ctx);
    if (!self->shouldRender.exchange(true))
    {
        self->renderWakeupCondition.notify_one();
    }
}

void MpvPlayer::mpvRenderThread()
{

    SetColorspace(GetDefaultColorspace());

    while (!renderThreadQuit)
    {
        {
            std::unique_lock<std::mutex> lock(renderWakeupMutex);
            renderWakeupCondition.wait(
                lock, [this]()
                { return shouldRender.load() || renderThreadQuit.load(); });

            if (renderThreadQuit)
                break;

            if (renderInProgress)
            {
                shouldRender = true;
                continue;
            }
            renderInProgress = true;
            shouldRender = false;
        }

        // Pace presents to the display refresh while idle. During playback mpv
        // blocks for each frame's target time, so presents already land one per
        // vblank; without video nothing throttles us and a resize would commit
        // buffers at ~300fps, which makes the compositor's interactive resize
        // stutter. Coalesced wakeups (renderInProgress above) accumulate during
        // the sleep, so we still render the latest size when we wake.
        if (playerState != PlayerState::PLAYING)
        {
            const auto interval =
                std::chrono::nanoseconds(idlePresentIntervalNs.load());
            const auto elapsed = std::chrono::steady_clock::now() - lastPresentTime;
            if (elapsed < interval)
            {
                std::this_thread::sleep_for(interval - elapsed);
            }
        }

        // rendering UI
        auto windowBottomLeftPoint = uiRenderCallback();

        if (needsResize)
        {
            vulkanInstance->ResizeSwapchain(width, height);
            needsResize = false;
        }

        {
            std::lock_guard<std::mutex> lock(colorspaceMutex);
            pl_swapchain_colorspace_hint(vulkanInstance->GetPlSwapchain(),
                                         &colorspace);
        }

        pl_swapchain_frame frame = {};
        if (!pl_swapchain_start_frame(vulkanInstance->GetPlSwapchain(), &frame))
        {
            spdlog::error("[render] failed to get swapchain frame!");
            renderInProgress = false;
            continue;
        }

        // Always render video if playing, regardless of what triggered the render
        mpvRenderFrame(&frame, windowBottomLeftPoint);
        if (playerState != PlayerState::PLAYING)
        {
            // Not playing and UI-triggered render
            vulkanInstance->DrawBackgroundFrame(&frame);
        }
        vulkanInstance->DrawUI(&frame);

        pl_swapchain_submit_frame(vulkanInstance->GetPlSwapchain());
        pl_swapchain_swap_buffers(vulkanInstance->GetPlSwapchain());
        mpv_render_context_report_swap(mpvRenderContext);
        lastPresentTime = std::chrono::steady_clock::now();
        renderInProgress = false;
    }
}

bool MpvPlayer::mpvRenderFrame(pl_swapchain_frame *frame,
                               const ImRect &desktopRect)
{
    mpv_render_rect_t rect = {
        .x0 = (int)desktopRect.Min.x,
        .y0 = (int)desktopRect.Min.y,
        .x1 = (int)desktopRect.Max.x,
        .y1 = (int)desktopRect.Max.y,
    };

    int block = 1;
    mpv_render_param render_params[] = {
        { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_OPTIONS,
          (void *)placeboOptions },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_FRAME,
          (void *)frame },
        { (enum mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_VIEWPORT,
          &rect },
        { MPV_RENDER_PARAM_INVALID, 0 }
    };

    mpv_render_context_update(mpvRenderContext);
    mpv_render_context_render(mpvRenderContext, render_params);

    return true;
}

void MpvPlayer::Render()
{
    shouldRender = true;
    renderWakeupCondition.notify_one();
}

void MpvPlayer::SetIdlePresentRate(int hz)
{
    if (hz > 0)
    {
        idlePresentIntervalNs = 1'000'000'000LL / hz;
    }
}

void MpvPlayer::SetSize(int width, int height)
{
    if (width == this->width && height == this->height)
        return;
    needsResize = true;
    this->width = width;
    this->height = height;
    Render();
}
void MpvPlayer::Play()
{
    ChannelPtr channel;
    {
        std::lock_guard<std::mutex> _{ currentlyPlayingChannelMutex };
        channel = currentlyPlayingChannel;
    }
    if (!channel || playerState == PlayerState::PLAYING)
        return;
    Play(std::move(channel));
}
void MpvPlayer::Stop()
{
    stopRequested = true;
    const char *cmdStop[] = { "stop", nullptr };
    mpv_command_async(mpv, STOP_COMMAND_REPLY_USERDATA, cmdStop);
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
    if (!channel)
        return;
    ChannelPtr playing;
    {
        std::lock_guard<std::mutex> _{ currentlyPlayingChannelMutex };
        playing = currentlyPlayingChannel;
    }
    if (playerState == PlayerState::PLAYING && channel == playing)
        return;

    {
        std::lock_guard<std::mutex> _{ currentlyPlayingChannelMutex };
        currentlyPlayingChannel = std::move(channel);
    }
    stopRequested = false;
    auto requestId = ++pendingLoadRequestId;
    const char *cmdStop[] = { "stop", nullptr };
    mpv_command_async(mpv, requestId, cmdStop);
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
    osdVolumeTimer.expires_after(OSD_DURATION);
    osdVolumeTimer.async_wait(std::bind(&MpvPlayer::removeVolumeOsd, this, _1));
    volumeSignal(mute ? 0.0 : volume);
}
void MpvPlayer::VolumeIncrease()
{
    volume += 5.0;
    SetVolume(volume);
    volumeSignal(this->volume);
}
void MpvPlayer::VolumeDecrease()
{
    volume -= 5.0;
    if (volume < 0.0)
        volume = 0.0;
    SetVolume(volume);
    volumeSignal(this->volume);
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
    osdVolumeTimer.expires_after(OSD_DURATION);
    osdVolumeTimer.async_wait(std::bind(&MpvPlayer::removeVolumeOsd, this, _1));
}

void MpvPlayer::ClosedCaptions(const std::string &id)
{
    mpv_set_property_string(mpv, "sid", id.c_str());
}
void MpvPlayer::Screenshot()
{
    const char *cmd[] = { "screenshot", nullptr };
    mpv_command_async(mpv, SCREENSHOT_COMMAND_REPLY_USERDATA, cmd);
}
void MpvPlayer::SetScreenshotPath(const std::filesystem::path &path)
{
    std::string path_str = path.string();
    mpv_set_option_string(mpv, "screenshot-directory", path_str.c_str());
}
void MpvPlayer::SetScreenshotFormat(const std::string &format)
{
    mpv_set_option_string(mpv, "screenshot-format", format.c_str());
}
void MpvPlayer::SetScreenshotFileTemplate(const std::string &fileTemplate)
{
    mpv_set_option_string(mpv, "screenshot-template", fileTemplate.c_str());
}
void MpvPlayer::removeScreenshotOsd(const boost::system::error_code &ec)
{
    if (!ec)
    {
        NodeVariant node = NodeVariantMap{ { "name", { "osd-overlay" } },
                                           { "id", { SCREENSHOT_OSD_ID } },
                                           { "format", { "none" } },
                                           { "data", { "" } } };
        NodeBuilder builder{ node };
        mpv_command_node(mpv, builder.GetNode(), nullptr);
    }
}
void MpvPlayer::showScreenshotOsd(const std::string &filename, bool error)
{
    std::string data =
        fmt::format("{{\\an2\\fs36}}Saved screenshot: {}", filename);
    if (error)
    {
        data =
            fmt::format("{{\\an2\\fs36\\1c&H0000FF&}}Error saving screenshot");
    }
    NodeVariant node = NodeVariantMap{
        { "name", { "osd-overlay" } },  { "id", { SCREENSHOT_OSD_ID } },
        { "format", { "ass-events" } }, { "res_x", { width } },
        { "res_y", { height } },        { "data", { data } }
    };
    NodeBuilder builder{ node };
    mpv_command_node(mpv, builder.GetNode(), nullptr);
}
void MpvPlayer::SetColorspace(const pl_color_space &colorspace)
{
    std::lock_guard<std::mutex> _{ colorspaceMutex };
    this->colorspace = colorspace;
}
pl_color_space MpvPlayer::GetColorspace() const
{
    std::lock_guard<std::mutex> _{ colorspaceMutex };
    return colorspace;
}
pl_color_space MpvPlayer::GetDefaultColorspace() const
{
    return { .primaries = pl_color_primaries_guess(width, height),
             .transfer = PL_COLOR_TRC_SRGB,
             .hdr = pl_hdr_metadata_empty };
}
