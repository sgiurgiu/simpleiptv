#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <boost/asio/io_context.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "mpvplayer.h"

void runMainLoop(GLFWwindow* window, ImGuiIO& io);

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

    ImGui::StyleColorsDark();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);

    runMainLoop(window, io);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}

void runMainLoop(GLFWwindow* window, ImGuiIO& io)
{
#ifdef STV_DEBUG
    bool show_demo_window = true;
#endif
    boost::asio::io_context uiContext;
    auto work = boost::asio::make_work_guard(uiContext);
    auto executor = uiContext.get_executor();
    MpvPlayer player{ executor };

    player.setSizeAsync(1280, 720);
    player.initializeMpvGL();

    glfwSetWindowUserPointer(window, &player);

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow* window, int width, int height)
        {
            MpvPlayer* desktop =
                reinterpret_cast<MpvPlayer*>(glfwGetWindowUserPointer(window));
            desktop->setSizeAsync(width, height);
        });
    glfwSetWindowSize(window, 1280, 720);
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);

    player.play("/home/sergiu/metallica_seattle.avi");
    // Main loop
    bool done = false;

    while (!done)
    {
        done = glfwWindowShouldClose(window);

        // execute one unit of work in the UI thread
        uiContext.poll_one();

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_Q | GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::NewFrame();

        int windowWidth;
        int windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

#ifdef STV_DEBUG
        // 1. Show the big demo window (Most of the sample code is in
        // ImGui::ShowDemoWindow()! You can browse its code to learn more about

        // Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
#endif

        ImGui::Render();

        // rendering stuff
        player.render();

        // glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        //  const auto& backgroundColor = desktop->getBackgroundColor();
        //  glClearColor(backgroundColor.x, backgroundColor.y,
        //  backgroundColor.z, backgroundColor.w);
        // glClearColor(0.3f, 0.3f, 0.3f, 1.f);
        // glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    work.reset();
    uiContext.stop();
    glfwSetWindowUserPointer(window, nullptr);
}