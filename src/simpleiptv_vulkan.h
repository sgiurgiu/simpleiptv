#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <libplacebo/log.h>
#include <libplacebo/renderer.h>
#include <libplacebo/shaders.h>
#include <libplacebo/utils/frame_queue.h>
#include <libplacebo/vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

struct ImageData
{
    pl_tex tex = nullptr;
    std::string name;
};

class MpvPlayer;

class SimpleIPTVVulkan
{
public:
    SimpleIPTVVulkan();
    ~SimpleIPTVVulkan();
    void Initialize(const char** extensions,
                    int extensions_count,
                    GLFWwindow* window);

    pl_gpu GetPlGpu() const
    {
        return vulkan->gpu;
    }
    pl_swapchain GetPlSwapchain() const
    {
        return swapchain;
    }
    pl_log GetPlLog() const
    {
        return logger;
    }

    void WaitForIdle();
    static void CheckError(VkResult err);
    void DrawUI(pl_swapchain_frame* frame);
    void DrawBackgroundFrame(pl_swapchain_frame* frame);
    void ResizeSwapchain(int width, int height);
    void DestroyImageData(ImageData& image);
    ImageData CreateImageData(int width, int height, int channels, uint8_t* data);

    ImageData CreatePlayerBarImageData(int width,
                                       int height,
                                       int channels,
                                       uint8_t* data);
    void DestroyPlayerBarImageData(ImageData& imageData);

private:
    void drawImgui(pl_swapchain_frame* frame);
    void createVulkanInstance(const char** extensions,
                              int extensions_count,
                              GLFWwindow* window);
    void cleanupVulkan();
    void initImguiFont();
    void resizeSwapchain();

    void createCustomShader(pl_shader sh, pl_tex texture);
    void initPlCache();
    void destroyPlCache();
    static int
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData);

private:
    pl_vk_inst vk_instance = nullptr;
    pl_log logger = nullptr;
    pl_vulkan vulkan = nullptr;
    pl_swapchain swapchain = nullptr;
    pl_dispatch dispatch = nullptr;
    pl_cache placeboCache = nullptr;
    uint64_t placeboCacheSig = 0;
    std::filesystem::path cacheFile;

    VkSurfaceKHR surface = nullptr;

    GLFWwindow* window = nullptr;
    pl_vertex_attrib attribs_pl[3];
    pl_tex imguiFontTex = nullptr;
    std::unordered_set<pl_tex> customTextures;
    pl_tex playerBarTexture = nullptr;
    pl_tex channelsLogosAtlas = nullptr;

    std::mutex imguiRenderMutex;
};
