#pragma once

#include "simpleiptv_ui.h"
#include "mpvplayer.h"
#include "workers_provider.h"
#include "simpleiptv_vulkan.h"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <mutex>

#ifdef STV_UNIX
#include "dbus_mpris_service.h"
#endif

class SimpleIPTVCoordinator
{
public:
    SimpleIPTVCoordinator(boost::asio::io_context& uiContext,
                          WorkersProvider* workersProvider,
                          SimpleIPTVVulkan* vulkanInstance);
    void Render();
    void SetSize(int width, int height);
    void SetIdlePresentRate(int hz);
    bool ShouldQuit() const;
    // Desired fullscreen state set by the UI; the main loop polls this and
    // applies the actual GLFW window change.
    bool IsFullscreen() const;

    // Drains pending UI-thread work while holding uiStateMutex, so the
    // DisplayNode tree is never mutated while the render thread walks it.
    void PollUI(boost::asio::io_context& uiContext);

private:
    WorkersProvider* workersProvider;
    boost::asio::any_io_executor uiExecutor;
    // Serializes the render-thread UI walk (RenderDesktop) against UI-thread
    // tree mutations (PollUI). Declared before simpleiptv/mpvPlayer so it
    // outlives the render thread that captures it.
    std::mutex uiStateMutex;
    // Last UI layout rect, reused when a render frame skips the rebuild because
    // PollUI holds uiStateMutex. Touched only on the render thread.
    ImRect lastDesktopRect{};
    SimpleIPTVUI simpleiptv;
    MpvPlayer mpvPlayer;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif

};