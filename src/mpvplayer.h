#pragma once

#include <GL/gl.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>

struct mpv_handle;
struct mpv_render_context;
struct mpv_event;

enum class PlayerState
{
    STOPPED,
    PLAYING,
    PAUSED,
    LOADING_ERROR
};

class MpvPlayer
{
public:
    MpvPlayer(const boost::asio::any_io_executor& ui_executor);
    ~MpvPlayer();
    void initializeMpvGL();
    void render();
    void setSizeAsync(int width, int height);
    void play(const std::string& file);
    PlayerState getPlayerState() const;

private:
    void setSize(int width, int height);
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

private:
    const boost::asio::any_io_executor& ui_executor;
    boost::asio::steady_timer resize_timer;
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
    GLuint frameShaderProgram;
    GLint videoFrameUniformLocation;

    GLint shaderPositionAttribLocation;
    GLint shaderTextCoordinateLocation;

    GLuint VAO;
    GLuint buffs[2];

    PlayerState playerState = PlayerState::STOPPED;
};
