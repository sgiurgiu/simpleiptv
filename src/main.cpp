#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <boost/asio/io_context.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "dbconnection_pool.h"
#include "simpleiptv.h"
#include "utils.h"
#include "workers_provider.h"

void runMainLoop(GLFWwindow* window);

static void glfw_error_callback(int error, const char* description)
{
    spdlog::error("GLFW Error {}:{}", error, description);
}

static void GLAPIENTRY MessageCallback(GLenum source,
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
    spdlog::default_logger()->set_level(spdlog::level::trace);
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    // Create window with graphics context
    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Simple IPTV", nullptr, nullptr);
    if (window == nullptr)
    {
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    glewInit();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    auto appConfigFolder = Utils::GetAppConfigFolder();
    auto iniFilePathString = (appConfigFolder / "imgui.ini").string();
    io.IniFilename = iniFilePathString.c_str();

    ImGui::StyleColorsDark();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    // glEnable(GL_DEBUG_OUTPUT);
    // glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);

    Utils::LoadFonts();
    DatabaseConnections::Initialize();

    runMainLoop(window);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}

void runMainLoop(GLFWwindow* window)
{
#ifdef STV_DEBUG
    bool show_demo_window = true;
#endif
    boost::asio::io_context uiContext;
    auto work = boost::asio::make_work_guard(uiContext);
    WorkersProvider workersProvider;
    SimpleIPTV iptv{ uiContext, workersProvider };

    glfwSetWindowUserPointer(window, &iptv);

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow* window, int width, int height)
        {
            SimpleIPTV* desktop =
                reinterpret_cast<SimpleIPTV*>(glfwGetWindowUserPointer(window));
            desktop->setSize(width, height);
        });
    glfwSetWindowSize(window, 1280, 720);
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);

    // Main loop
    bool done = false;

    while (!done)
    {
        done = glfwWindowShouldClose(window);

        // execute one unit of work in the UI thread
        uiContext.poll_one();

        glfwPollEvents();

        if (iptv.shouldQuit())
        {
            glfwSetWindowShouldClose(window, true);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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

        glfwSwapBuffers(window);
    }
    work.reset();
    uiContext.stop();
    glfwSetWindowUserPointer(window, nullptr);
}
