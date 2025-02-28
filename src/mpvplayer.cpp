#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <GL/glew.h>

#include "mpvplayer.h"

#include <GLFW/glfw3.h>
#include <boost/asio/post.hpp>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <stdexcept>

#ifdef STV_UNIX
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif
#include <format>

#include "mpvhelper.h"
#include "utils.h"
#include <spdlog/spdlog.h>

namespace
{

static void *get_proc_address(void *, const char *name)
{
    return (void *)glfwGetProcAddress(name);
}

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
    mpv_set_property_string(mpv, "msg-level", "all=v");
    mpv_set_property_string(mpv, "sub-create-cc-track", "yes");
    mpv_set_property_string(mpv, "input-default-bindings", "no");
    mpv_set_property_string(mpv, "config", "no");
    mpv_set_property_string(mpv, "input-vo-keyboard", "no");
    mpv_set_property_string(mpv, "vo", "libmpv");

#ifdef STV_UNIX
    mpv_set_property_string(mpv, "ao", "pipewire");
#else
    mpv_set_property_string(mpv, "ao", "auto");
#endif
    mpv_set_property_string(mpv, "hwdec", "auto");
    mpv_observe_property(mpv, 0, "height", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "width", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    int64_t cacheSecs = 30;
    mpv_set_property(mpv, "cache-secs", MPV_FORMAT_INT64, &cacheSecs);
    mpv_set_property(mpv, "demuxer-readahead-secs", MPV_FORMAT_INT64, &cacheSecs);

    this->workersProvider->GetProxyRepository()->LoadConfiguredProxy(
        [this](auto proxy)
        {
            if (proxy.use)
            {
                auto proxyUrl =
                    std::format("http://{}:{}", proxy.host, proxy.port);
                mpv_set_option_string(mpv, "http-proxy", proxyUrl.c_str());
            }
            if (mpv_initialize(mpv) < 0)
                throw std::runtime_error("could not initialize mpv context");
            int errorCode =
                mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
            if (errorCode)
            {
                spdlog::error("Cannot set volume: {}",
                              mpv_error_string(errorCode));
            }
        },
        ui_executor);
    mpv_set_option_string(mpv, "hwdec", "auto");
    // mpv_set_option_string(mpv, "gpu-debug", "true");

    double volMax = 150.0;
    mpv_set_property(mpv, "volume-max", MPV_FORMAT_DOUBLE, &volMax);

    mpv_set_wakeup_callback(mpv, MpvPlayer::onMpvEvents, this);
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
    destroyFrameBuffers();
    glDeleteProgram(frameShaderProgram);
    glDeleteBuffers(2, buffs);
    glDeleteVertexArrays(1, &VAO);
    workersProvider->GetSleepService()->enableComputerSleep();
}

void MpvPlayer::InitializeMpvGL()
{
#ifdef STV_UNIX
    int platform = glfwGetPlatform();
    mpv_render_param display{ MPV_RENDER_PARAM_INVALID, nullptr };
    if (platform == GLFW_PLATFORM_X11)
    {
        display.type = MPV_RENDER_PARAM_X11_DISPLAY;
        display.data = glfwGetX11Display();
    }
    else if (platform == GLFW_PLATFORM_WAYLAND)
    {
        display.type = MPV_RENDER_PARAM_WL_DISPLAY;
        display.data = glfwGetWaylandDisplay();
    }
#endif
    mpv_opengl_init_params gl_params = { get_proc_address, nullptr };
    int mpv_advanced_control = 0;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE,
          const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_params },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &mpv_advanced_control },
#ifdef STV_UNIX
        { display },
#endif
        { MPV_RENDER_PARAM_INVALID, 0 }
    };
    mpv_render_context_create(&mpvRenderContext, mpv, params);

    mpv_render_context_set_update_callback(mpvRenderContext,
                                           &MpvPlayer::mpvRenderUpdate, this);
    compileShaders();
    createFrameBuffers();
    initializeVAO();
}

void MpvPlayer::compileShaders()
{
    GLuint vertexShader;
    GLuint fragmentShader;
    int success = 0;
    char infoLog[1024];
    const char *frameVertexShaderTextCStr = frameVertexShaderText.c_str();
    const char *frameFragmentShaderTextCStr = frameFragmentShaderText.c_str();

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &frameVertexShaderTextCStr, NULL);
    glCompileShader(vertexShader);
    // print compile errors if any
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), NULL, infoLog);
        throw std::runtime_error(std::format(
            "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n{}", infoLog));
    };

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &frameFragmentShaderTextCStr, NULL);
    glCompileShader(fragmentShader);
    // print compile errors if any
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), NULL, infoLog);
        throw std::runtime_error(std::format(
            "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n{}", infoLog));
    };

    frameShaderProgram = glCreateProgram();
    glAttachShader(frameShaderProgram, vertexShader);
    glAttachShader(frameShaderProgram, fragmentShader);
    glLinkProgram(frameShaderProgram);

    // print linking errors if any
    glGetProgramiv(frameShaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(frameShaderProgram, sizeof(infoLog), NULL, infoLog);
        throw std::runtime_error(
            std::format("ERROR::SHADER::PROGRAM::LINKING_FAILED\\n{}", infoLog));
    }

    // delete the shaders as they're linked into our program now and no longer
    // necessary
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    videoFrameUniformLocation =
        glGetUniformLocation(frameShaderProgram, "videoFrame");
    shaderPositionAttribLocation =
        glGetAttribLocation(frameShaderProgram, "position");
    shaderTextCoordinateLocation =
        glGetAttribLocation(frameShaderProgram, "inputTextureCoordinate");
}

void MpvPlayer::createFrameBuffers()
{
    destroyFrameBuffers();

    glGenFramebuffers(1, &mediaFramebufferObject);
    glBindFramebuffer(GL_FRAMEBUFFER, mediaFramebufferObject);

    glGenTextures(1, &mediaFrameTexture);
    glBindTexture(GL_TEXTURE_2D, mediaFrameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)frameWidth, (int)frameHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           mediaFrameTexture, 0);

    /*glGenRenderbuffers(1, &mediaFrameRenderBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, mediaFrameRenderBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, mediaFrameRenderBufferObject);*/

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    // glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
void MpvPlayer::destroyFrameBuffers()
{
    if (mediaFrameTexture > 0)
    {
        glDeleteTextures(1, &mediaFrameTexture);
        mediaFrameTexture = 0;
    }
    /*if (mediaFrameRenderBufferObject > 0)
    {
        glDeleteRenderbuffers(1, &mediaFrameRenderBufferObject);
        mediaFrameRenderBufferObject = 0;
    }*/

    if (mediaFramebufferObject > 0)
    {
        glDeleteFramebuffers(1, &mediaFramebufferObject);
        mediaFramebufferObject = 0;
    }
}
void MpvPlayer::rescaleFrameBuffers()
{
    // destroyFrameBuffers();
    // createFrameBuffers();
    if (!mediaFramebufferObject || !mediaFrameTexture)
    {
        // createFrameBuffers();
        return;
    }

    glDeleteTextures(1, &mediaFrameTexture);
    glGenTextures(1, &mediaFrameTexture);
    glBindTexture(GL_TEXTURE_2D, mediaFrameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)frameWidth, (int)frameHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, mediaFramebufferObject);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           mediaFrameTexture, 0);

    /*glBindRenderbuffer(GL_RENDERBUFFER, mediaFrameRenderBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, mediaFrameRenderBufferObject);*/

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
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
                mpv, std::format("track-list/{}/type", i).c_str());
            if (type == std::string("sub"))
            {
                char *id = mpv_get_property_string(
                    mpv, std::format("track-list/{}/id", i).c_str());
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
    boost::asio::post(self->ui_executor,
                      std::bind(&MpvPlayer::updateDisplay, self));
}

void MpvPlayer::updateDisplay()
{
    mpvRenderFrame();
}

void MpvPlayer::mpvRenderFrame()
{
    uint64_t flags = mpv_render_context_update(mpvRenderContext);
    if (!(flags & MPV_RENDER_UPDATE_FRAME))
    {
        return;
    }

    mpv_opengl_fbo mpfbo{ (int)mediaFramebufferObject, frameWidth, frameHeight,
                          GL_RGBA };
    int flip_y = 0;

    mpv_render_param params[] = { { MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo },
                                  { MPV_RENDER_PARAM_FLIP_Y, &flip_y },
                                  { MPV_RENDER_PARAM_SKIP_RENDERING,
                                    &skipRendering },
                                  { MPV_RENDER_PARAM_INVALID, 0 } };
    mpv_render_context_render(mpvRenderContext, params);
}
void MpvPlayer::initializeVAO()
{
    // clang-format off
        static const GLfloat squareVertices[] = {
            -1.0f, -1.0f, 
            1.0f, -1.0f, 
            -1.0f, 1.0f, 
            1.0f, 1.0f,
        };

        static const GLfloat textureVertices[] = {
            0.0f, 1.0f, 
            1.0f, 1.0f, 
            0.0f, 0.0f, 
            1.0f, 0.0f,
        };
    // clang-format on

    glGenBuffers(2, buffs);
    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glEnableVertexAttribArray(shaderPositionAttribLocation);
    glVertexAttribPointer(shaderPositionAttribLocation, 2, GL_FLOAT, 0, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glEnableVertexAttribArray(shaderTextCoordinateLocation);
    glVertexAttribPointer(shaderTextCoordinateLocation, 2, GL_FLOAT, 0, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}
void MpvPlayer::Render(const ImVec2 &windowsSize)
{
    if (lastWindowSize.x != windowsSize.x || lastWindowSize.y != windowsSize.y)
    {
        lastWindowSize = windowsSize;
        frameWidth = width - lastWindowSize.x;
        frameHeight = height - lastWindowSize.y;
        rescaleFrameBuffers();
        mpvRenderFrame();
    }
    glViewport(lastWindowSize.x, lastWindowSize.y, frameWidth, frameHeight);

    glUseProgram(frameShaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mediaFrameTexture);

    glUniform1i(videoFrameUniformLocation, 0);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, shaderPositionAttribLocation,
                     buffs[0]);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, shaderTextCoordinateLocation,
                     buffs[1]);

    glBindVertexArray(VAO);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindVertexArray(0);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, shaderPositionAttribLocation,
                     0);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, shaderTextCoordinateLocation,
                     0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void MpvPlayer::SetSize(int width, int height)
{
    if (width == this->width && height == this->height)
        return;
    this->width = width;
    this->height = height;
    frameWidth = width - lastWindowSize.x;
    frameHeight = height - lastWindowSize.y;

    rescaleFrameBuffers();
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
    rescaleFrameBuffers();
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
    rescaleFrameBuffers();
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
        { "data", { std::format("{{\\an9\\fs36}}Mute {}", mute ? "on" : "off") } }
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
        { "data", { std::format("{{\\an9\\fs36}}Volume {}", (int)this->volume) } }
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