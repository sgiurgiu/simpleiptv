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
    bool ShouldQuit() const;

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
    SimpleIPTVUI simpleiptv;
    MpvPlayer mpvPlayer;
#ifdef STV_UNIX
    std::shared_ptr<MprisService> mprisService;
#endif

};