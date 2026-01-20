#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>
#include <vulkan/vulkan.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "dbconnection_pool.h"
#include "simpleiptv.h"
#include "simpleiptv_vulkan.h"
#include "stv_utils.h"
#include "workers_provider.h"

#include <openssl/crypto.h>

#include <boost/url.hpp>

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

    auto vulkanInstance = std::make_unique<SimpleIPTVVulkan>();
    {
        std::set<std::string> extensions;
        uint32_t extensions_count = 0;
        const char** glfwExtensions =
            glfwGetRequiredInstanceExtensions(&extensions_count);
        for (uint32_t i = 0; i < extensions_count; i++)
            extensions.insert(glfwExtensions[i]);

        vulkanInstance->Initialize(std::move(extensions), window);
    }
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, nullptr, nullptr);

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // ImGui::StyleColorsLight();
    /*ImGui::GetStyle().Colors[ImGuiCol_WindowBg] =
        ImVec4(0.56f, 0.56f, 0.56f, 0.94f);*/
    // Setup Platform/Renderer backends
    // ImGui_ImplGlfw_InitForOpenGL(window, true);
    std::thread uiThread([window, &workersProvider, vk = vulkanInstance.get()]()
                         { runMainLoop(window, workersProvider, vk); });

    uiThread.join();

    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    settingsRepository->SetWindowWidth(width);
    settingsRepository->SetWindowHeight(height);

    // Cleanup

    ImGui_ImplGlfw_Shutdown();

    glfwDestroyWindow(window);
}

void runMainLoop(GLFWwindow* window,
                 WorkersProvider& workersProvider,
                 SimpleIPTVVulkan* vulkanInstance)
{
#ifdef STV_DEBUG
    bool show_demo_window = true;
#endif
    boost::asio::io_context uiContext;
    auto work = boost::asio::make_work_guard(uiContext);
    SimpleIPTV iptv{ uiContext, &workersProvider, vulkanInstance };

    glfwSetWindowUserPointer(window, &iptv);

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow* window, int width, int height)
        {
            SimpleIPTV* desktop =
                reinterpret_cast<SimpleIPTV*>(glfwGetWindowUserPointer(window));
            desktop->setSize(width, height);
            //  glfwPostEmptyEvent(); // Wake up event loop
        });

    // Main loop
    bool done = false;

    while (!done)
    {
        done = glfwWindowShouldClose(window);
        // spdlog::debug("before - glfwPollEvents");
        glfwPollEvents();
        // spdlog::debug("after glfwPollEvents");

        if (iptv.shouldQuit())
        {
            glfwSetWindowShouldClose(window, true);
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // execute one unit of work in the UI thread
        // this includes the mpv render part
        uiContext.poll_one();

#ifdef STV_DEBUG
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
#endif

        // rendering stuff
        auto windowBottomLeftPoint = iptv.showDesktop();

        ImGui::Render();

        iptv.Render(windowBottomLeftPoint);
        // std::this_thread::sleep_for(std::chrono::milliseconds{ 16 } -
        //                             durationSpentDrawing);
        // start = std::chrono::steady_clock::now();
    }
    work.reset();
    glfwSetWindowUserPointer(window, nullptr);
}
