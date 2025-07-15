#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <boost/asio/io_context.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>

#include "dbconnection_pool.h"
#include "simpleiptv.h"
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
                 boost::asio::io_context& uiContext);
void startGraphicalInterface();

static void glfw_error_callback(int error, const char* description)
{
    spdlog::error("GLFW Error {}:{}", error, description);
}

static void GLAPIENTRY GLMessageCallback(GLenum source,
                                         GLenum type,
                                         GLuint id,
                                         GLenum severity,
                                         GLsizei length,
                                         const GLchar* message,
                                         const void* userParam)
{
    (void)source;
    (void)id;
    (void)length;
    (void)userParam;
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        spdlog::error("GL CALLBACK: type:{}, severity:{}, message:{}", type,
                      severity, message);
        break;
    default:
        spdlog::info("GL CALLBACK: type:{}, severity:{}, message:{}", type,
                     severity, message);
        break;
    }
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
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

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

    std::thread uiThread([]() { startGraphicalInterface(); });

    uiThread.join();

    ImGui::DestroyContext();
    glfwTerminate();

    return EXIT_SUCCESS;
}

void startGraphicalInterface()
{
    boost::asio::io_context uiContext;
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
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable vsync

    glewInit();

    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    /*ImGui::GetStyle().Colors[ImGuiCol_WindowBg] =
        ImVec4(0.56f, 0.56f, 0.56f, 0.94f);*/
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    // glEnable(GL_DEBUG_OUTPUT);
    // glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLMessageCallback, nullptr);

    runMainLoop(window, workersProvider, uiContext);

    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    settingsRepository->SetWindowWidth(width);
    settingsRepository->SetWindowHeight(height);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    glfwDestroyWindow(window);
}

void runMainLoop(GLFWwindow* window,
                 WorkersProvider& workersProvider,
                 boost::asio::io_context& uiContext)
{
#ifdef STV_DEBUG
    bool show_demo_window = true;
#endif
    auto work = boost::asio::make_work_guard(uiContext);
    SimpleIPTV iptv{ uiContext, &workersProvider };

    glfwSetWindowUserPointer(window, &iptv);

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow* window, int width, int height)
        {
            SimpleIPTV* desktop =
                reinterpret_cast<SimpleIPTV*>(glfwGetWindowUserPointer(window));
            desktop->setSize(width, height);
        });

    //    glfwSetWindowSize(window, 1280, 720);
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);

    // Main loop
    bool done = false;
    auto start = std::chrono::steady_clock::now();
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
        {
            int width;
            int height;
            glfwGetFramebufferSize(window, &width, &height);
            iptv.setSize(width, height);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // execute one unit of work in the UI thread
        // this includes the mpv render part
        uiContext.poll_one();

#ifdef STV_DEBUG
        // 1. Show the big demo window (Most of the sample code is in
        // ImGui::ShowDemoWindow()! You can browse its code to learn more about

        // Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
#endif

        // rendering stuff
        iptv.showDesktop();

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // on wayland with EGL, eglSwapBuffers blocks when the window is
        // minimized when glfwSwapInterval(1) (vsync enabled).
        // Now, that would be fine, but if we're playing video
        // mpv complains loudly if we're not calling mpv_render_context_render
        // The worst thing is, in wayland, I have no way to know if the window
        // is minimized. Way to go wayland.
        // Disabling vsync, we're not blocking here anymore
        // but then we have to do the wait ourselves
        // Ideally, we would try to match the monitor's (which one's?) refresh
        // rate but ... that would be a bit more complicated, so for now just
        // set it at 16ms (60Hz).
        glfwSwapBuffers(window);

        auto end = std::chrono::steady_clock::now();
        auto durationSpentDrawing = end - start;
        std::this_thread::sleep_for(std::chrono::milliseconds{ 16 } -
                                    durationSpentDrawing);
        start = std::chrono::steady_clock::now();
    }
    work.reset();
    glfwSetWindowUserPointer(window, nullptr);
}
