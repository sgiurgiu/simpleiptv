#include <memory>
#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <iostream>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <vulkan/vulkan.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "dbconnection_pool.h"
#include "images/icon.h"
#include "simpleiptv_coordinator.h"
#include "simpleiptv_vulkan.h"
#include "stv_utils.h"
#include "workers_provider.h"

#include <boost/url.hpp>
#include <openssl/crypto.h>

struct PreloadOpensslCrypto
{
    PreloadOpensslCrypto()
    {
        // workaround to some linux distributions (looking at you Fedora)
        // having options that are not normally recognized by vcpkg openssl
        // They basically really want us to use the system openssl
        // which is cool and all, but I would prefer to statically link it
        int ret = OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr);
        std::cout << "OPENSSL_init_crypto:" << ret << std::endl;
    }
};

static PreloadOpensslCrypto _dummy;

void runMainLoop(GLFWwindow* window,
                 WorkersProvider& workersProvider,
                 SimpleIPTVVulkan* vulkanInstance);
void startGraphicalInterface();

static void glfw_error_callback(int error, const char* description)
{
    spdlog::error("GLFW Error {}:{}", error, description);
}

#if defined(WIN32_WINMAIN)
int WINAPI WinMain(_In_ HINSTANCE hInstance,
                   _In_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine,
                   _In_ int nCmdShow)
#else
int main(int /*argc*/, char** /*argv*/)
#endif
{
#ifdef STV_DEBUG
    spdlog::default_logger()->set_level(spdlog::level::trace);
#else
    spdlog::default_logger()->set_level(spdlog::level::err);
#endif

    DatabaseConnections::Initialize();

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        spdlog::error("Cannot init GLFW");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Don't iconify the fullscreen window when it loses focus (e.g. alt-tab);
    // stay fullscreen like every other video player.
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    auto appConfigFolder = Utils::GetAppConfigFolder();
    auto iniFilePathString = (appConfigFolder / "imgui.ini").string();
    io.IniFilename = iniFilePathString.c_str();

    Utils::LoadFonts();

    startGraphicalInterface();

    ImGui::DestroyContext();
    glfwTerminate();

    return EXIT_SUCCESS;
}

void startGraphicalInterface()
{
    WorkersProvider workersProvider;
    auto settingsRepository = workersProvider.GetSettingsRepository();
    // Create window with graphics context
#ifdef STV_DEBUG
    std::string title = "Simple IPTV - Debug";
#else
    std::string title = "Simple IPTV";
#endif
    GLFWwindow* window =
        glfwCreateWindow(settingsRepository->GetWindowWidth(1280),
                         settingsRepository->GetWindowHeight(720),
                         title.c_str(), nullptr, nullptr);
    if (window == nullptr)
    {
        spdlog::error("Cannot create Window");
        return;
    }
    // Check for Vulkan support
    if (!glfwVulkanSupported())
    {
        spdlog::critical("Vulkan not supported");
        glfwDestroyWindow(window);
        return;
    }

    {
        int width = 0;
        int height = 0;
        int channels = 0;
        auto imageData = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(______icons_simpleiptv_icon_32_png),
            ______icons_simpleiptv_icon_32_png_len, &width, &height, &channels,
            STBI_rgb_alpha);
        GLFWimage images[1];
        images[0].pixels = imageData;
        images[0].width = width;
        images[0].height = height;
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(imageData);
    }

    auto vulkanInstance = std::make_unique<SimpleIPTVVulkan>();
    {
        uint32_t extensions_count = 0;
        const char** glfwExtensions =
            glfwGetRequiredInstanceExtensions(&extensions_count);

        vulkanInstance->Initialize(glfwExtensions, extensions_count, window);
    }
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, nullptr, nullptr);

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // ImGui::StyleColorsLight();
    /*ImGui::GetStyle().Colors[ImGuiCol_WindowBg] =
        ImVec4(0.56f, 0.56f, 0.56f, 0.94f);*/

    runMainLoop(window, workersProvider, vulkanInstance.get());

    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    settingsRepository->SetWindowWidth(width);
    settingsRepository->SetWindowHeight(height);

    // Cleanup

    ImGui_ImplGlfw_Shutdown();

    glfwDestroyWindow(window);
}

// Picks the monitor whose area contains the window center, falling back to
// the primary monitor. On Wayland window positions are always (0,0), so this
// degrades to the monitor at the origin; the compositor decides fullscreen
// placement there anyway.
static GLFWmonitor* pickMonitorForWindow(GLFWwindow* window)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitors && monitorCount > 0)
    {
        int wx = 0;
        int wy = 0;
        int ww = 0;
        int wh = 0;
        glfwGetWindowPos(window, &wx, &wy);
        glfwGetWindowSize(window, &ww, &wh);
        const int cx = wx + ww / 2;
        const int cy = wy + wh / 2;
        for (int i = 0; i < monitorCount; ++i)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
            {
                continue;
            }
            int mx = 0;
            int my = 0;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            if (cx >= mx && cx < mx + mode->width && cy >= my &&
                cy < my + mode->height)
            {
                return monitors[i];
            }
        }
    }
    return glfwGetPrimaryMonitor();
}

void runMainLoop(GLFWwindow* window,
                 WorkersProvider& workersProvider,
                 SimpleIPTVVulkan* vulkanInstance)
{
    vulkanInstance->WaitForIdle();
    boost::asio::io_context uiContext;
    auto work = boost::asio::make_work_guard(uiContext);
    SimpleIPTVCoordinator coordinator{ uiContext, &workersProvider,
                                       vulkanInstance };
    glfwSetWindowUserPointer(window, &coordinator);
    // Cap idle (no-video) rendering to the monitor refresh rate so a resize
    // doesn't flood the compositor with unsynced buffers. Falls back to 60Hz if
    // the refresh rate can't be queried.
    if (const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor()))
    {
        coordinator.SetIdlePresentRate(mode->refreshRate);
    }
    {
        int width;
        int height;
        glfwGetWindowSize(window, &width, &height);
        coordinator.SetSize(width, height);
    }

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow* window, int width, int height)
        {
            SimpleIPTVCoordinator* coordinator =
                reinterpret_cast<SimpleIPTVCoordinator*>(
                    glfwGetWindowUserPointer(window));
            if (!coordinator)
            {
                return;
            }
            coordinator->SetSize(width, height);
        });

    // Windowed geometry saved when entering fullscreen, restored on exit.
    struct
    {
        int x = 0;
        int y = 0;
        int w = 1280;
        int h = 720;
    } windowedGeometry;
    bool fullscreenApplied = false;
    const bool isWayland = glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;

    // Cursor auto-hide while fullscreen: hidden on entry, shown on movement,
    // hidden again after the delay. The UI sets
    // ImGuiConfigFlags_NoMouseCursorChange while fullscreen so the ImGui
    // backend doesn't re-assert a visible cursor each frame over these calls.
    constexpr std::chrono::seconds cursorAutoHideDelay{ 5 };
    constexpr std::chrono::milliseconds cursorSettleGrace{ 500 };
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
    auto lastCursorMove = std::chrono::steady_clock::now();
    auto fullscreenEnteredAt = lastCursorMove;
    bool cursorHidden = false;

    // Main loop
    bool done = false;
    // Idle UI refresh interval (seconds). glfwWaitEventsTimeout blocks until an
    // input/window event or this timeout, so interaction wakes us immediately
    // while an untouched screen redraws at this rate instead of a constant 60
    // fps. The tick still drives time-based UI (volume auto-hide, tooltip
    // delays, EPG rollover). Video is unaffected: it drives the render thread
    // directly via mpvRenderUpdate.
    constexpr double idleTick = 1.0 / 15.0;
    while (!done)
    {
        done = glfwWindowShouldClose(window);
        glfwWaitEventsTimeout(idleTick);

        if (coordinator.ShouldQuit())
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Run pending UI work under the coordinator's lock so the render thread
        // never walks the DisplayNode tree while it is being mutated here.
        coordinator.PollUI(uiContext);

        // Fullscreen intent is set on the render thread (key handling in the
        // UI), but the GLFW window can only be changed from this thread.
        const bool wantFullscreen = coordinator.IsFullscreen();
        if (wantFullscreen != fullscreenApplied)
        {
            if (wantFullscreen)
            {
                if (!isWayland)
                {
                    // Not available on Wayland (and would spam the GLFW error
                    // callback); the compositor restores placement there.
                    glfwGetWindowPos(window, &windowedGeometry.x,
                                     &windowedGeometry.y);
                }
                glfwGetWindowSize(window, &windowedGeometry.w,
                                  &windowedGeometry.h);
                // On Wayland the patched GLFW passes a NULL output so the
                // compositor picks the monitor the window is on; the monitor
                // choice only matters on X11.
                GLFWmonitor* monitor = isWayland ? glfwGetPrimaryMonitor()
                                                 : pickMonitorForWindow(window);
                if (const GLFWvidmode* mode = glfwGetVideoMode(monitor))
                {
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width,
                                         mode->height, mode->refreshRate);
                }
                glfwGetCursorPos(window, &lastCursorX, &lastCursorY);
                fullscreenEnteredAt = std::chrono::steady_clock::now();
                // Hide immediately: pretend the last move was long ago.
                lastCursorMove = fullscreenEnteredAt - cursorAutoHideDelay;
            }
            else
            {
                glfwSetWindowMonitor(window, nullptr, windowedGeometry.x,
                                     windowedGeometry.y, windowedGeometry.w,
                                     windowedGeometry.h, 0);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                cursorHidden = false;
            }
            fullscreenApplied = wantFullscreen;
#ifdef STV_UNIX
            workersProvider.GetMprisService()->SetCurrentFullscreen(
                wantFullscreen);
#endif
        }

        if (fullscreenApplied)
        {
            double cx = 0.0;
            double cy = 0.0;
            glfwGetCursorPos(window, &cx, &cy);
            const auto now = std::chrono::steady_clock::now();
            if (cx != lastCursorX || cy != lastCursorY)
            {
                lastCursorX = cx;
                lastCursorY = cy;
                // The fullscreen resize shifts the window-relative cursor
                // coordinates; don't count that as user movement.
                if (now - fullscreenEnteredAt > cursorSettleGrace)
                {
                    lastCursorMove = now;
                }
            }
            const bool hide = now - lastCursorMove >= cursorAutoHideDelay;
            if (hide != cursorHidden)
            {
                cursorHidden = hide;
                spdlog::debug("fullscreen cursor {}", hide ? "hidden" : "shown");
            }

            // Re-issued every tick: a no-op while the mode is unchanged, and
            // it recovers from any cursor write the render thread made during
            // the transition frames.
            glfwSetInputMode(window, GLFW_CURSOR,
                             hide ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
        }

        coordinator.Render();
    }

    // Leave fullscreen before shutdown so the window size persisted by
    // startGraphicalInterface is the windowed one, not the monitor's.
    if (fullscreenApplied)
    {
        glfwSetWindowMonitor(window, nullptr, windowedGeometry.x,
                             windowedGeometry.y, windowedGeometry.w,
                             windowedGeometry.h, 0);
    }

    // Orderly shutdown. uiContext and coordinator live on this stack, but the
    // worker pools (in workersProvider) outlive them. Stop background work first
    // so nothing posts into a destroyed io_context, then drain any handlers the
    // in-flight workers queued. PollUI drains under uiStateMutex, since the
    // render thread keeps walking the tree until the coordinator is destroyed.
    workersProvider.StopWorkers();
    coordinator.PollUI(uiContext);

    work.reset();
    glfwSetWindowUserPointer(window, nullptr);
}
