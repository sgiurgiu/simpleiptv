#include "mpvplayer.h"

#include <GLFW/glfw3.h>
#include <boost/asio/post.hpp>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <stdexcept>

#ifdef STV_UNIX
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif
#include <fmt/format.h>

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

enum
{
    ATTRIB_VERTEX,
    ATTRIB_TEXTUREPOSITON,
    ATTRIB_COLOR,
    NUM_ATTRIBUTES
};

} // namespace

MpvPlayer::MpvPlayer(const boost::asio::any_io_executor &ui_executor)
: ui_executor{ ui_executor }
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
    mpv_observe_property(mpv, 0, "height", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "width", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    int64_t cacheSecs = 30;
    mpv_set_property(mpv, "cache-secs", MPV_FORMAT_INT64, &cacheSecs);
    mpv_set_property(mpv, "demuxer-readahead-secs", MPV_FORMAT_INT64, &cacheSecs);

    // mpv_set_option_string(mpv, "http-proxy", proxyUrl.toStdString().c_str());

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    mpv_set_option_string(mpv, "hwdec", "auto");
    // mpv_set_option_string(mpv, "gpu-debug", "true");
    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
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
}

void MpvPlayer::initializeMpvGL()
{
#ifdef STV_UNIX
    mpv_render_param display{ MPV_RENDER_PARAM_X11_DISPLAY, glfwGetX11Display() };
#endif
    mpv_opengl_init_params gl_params = { get_proc_address, nullptr };
    int mpv_advanced_control = 0;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE,
          const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_params },
    // { MPV_RENDER_PARAM_ADVANCED_CONTROL, &mpv_advanced_control },
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
        throw std::runtime_error(fmt::format(
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
        throw std::runtime_error(fmt::format(
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
            fmt::format("ERROR::SHADER::PROGRAM::LINKING_FAILED\\n{}", infoLog));
    }

    // delete the shaders as they're linked into our program now and no longer
    // necessary
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    videoFrameUniformLocation =
        glGetUniformLocation(frameShaderProgram, "videoFrame");
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)width, (int)height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           mediaFrameTexture, 0);

    glGenRenderbuffers(1, &mediaFrameRenderBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, mediaFrameRenderBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, mediaFrameRenderBufferObject);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
void MpvPlayer::destroyFrameBuffers()
{
    if (mediaFrameTexture > 0)
    {
        glDeleteTextures(1, &mediaFrameTexture);
        mediaFrameTexture = 0;
    }
    if (mediaFrameRenderBufferObject > 0)
    {
        glDeleteRenderbuffers(1, &mediaFrameRenderBufferObject);
        mediaFrameRenderBufferObject = 0;
    }

    if (mediaFramebufferObject > 0)
    {
        glDeleteFramebuffers(1, &mediaFramebufferObject);
        mediaFramebufferObject = 0;
    }
}
void MpvPlayer::rescaleFrameBuffers()
{
    if (!mediaFramebufferObject)
    {
        // createFrameBuffers();
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, mediaFramebufferObject);
    glBindTexture(GL_TEXTURE_2D, mediaFrameTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           mediaFrameTexture, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, mediaFrameRenderBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, mediaFrameRenderBufferObject);

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
        if (end_file->error < 0)
        {
            // notifyOfErrors(end_file->error);
        }
    }
    break;
    case MPV_EVENT_FILE_LOADED:
        // startRenderingMedia();
        // emit fileLoaded();
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
                      std::bind(&MpvPlayer::mpvRenderFrame, self));
}
void MpvPlayer::mpvRenderFrame()
{
    uint64_t flags = mpv_render_context_update(mpvRenderContext);
    if (!(flags & MPV_RENDER_UPDATE_FRAME))
    {
        return;
    }

    mpv_opengl_fbo mpfbo{ (int)mediaFramebufferObject, width, height, GL_RGBA };
    int flip_y = 1;
    int skipRendering = 0;
    if (/*!shouldRenderMedia || window()->isMinimized()*/ false)
    {
        skipRendering = 1;
    }

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
            1.0f, 1.0f, 
            1.0f, 0.0f, 
            0.0f, 1.0f, 
            0.0f, 0.0f,
        };
    // clang-format on
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    GLuint buffs[2];
    glGenBuffers(2, buffs);
    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0,
                          textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
void MpvPlayer::render()
{
    if (mediaFrameTexture)
    {
        // glViewport(0, 0, width, height);
        //  glBindFramebuffer(GL_FRAMEBUFFER, mediaFramebufferObject);
        //    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        //    glClear(GL_COLOR_BUFFER_BIT);

        // glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glUseProgram(frameShaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mediaFrameTexture);

        glUniform1i(videoFrameUniformLocation, 0);

        /*glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0,
        squareVertices); glEnableVertexAttribArray(ATTRIB_VERTEX);
        glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0,
                              textureVertices);
        glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);*/

        glBindVertexArray(VAO);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);

        glUseProgram(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        /*ImGui::Image((void *)(intptr_t)mediaFrameTexture, ImVec2(width,
           height), ImVec2(0, 0), ImVec2(1, 1));*/

        /*glBindFramebuffer(GL_READ_FRAMEBUFFER, mediaFramebufferObject);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // if not already bound
        // glViewport(0, 0, width, height);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);*/
    }
}
void MpvPlayer::setSize(int width, int height)
{
    if (width == this->width && height == this->height)
        return;
    this->width = width;
    this->height = height;
    glViewport(0, 0, width, height);
    rescaleFrameBuffers();
}

void MpvPlayer::play(const std::string &file)
{
    const char *cmd[] = { "loadfile", file.c_str(), nullptr };
    mpv_command_async(mpv, 0, cmd);
}