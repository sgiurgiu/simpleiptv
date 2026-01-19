#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>
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

struct ImGuiDrawCommand
{
    ImTextureID textureId;
    const ImDrawCmd* pcmd = nullptr;
    int vertexOffset = 0;
    int indexOffset = 0;
};

class MpvPlayer;

class SimpleIPTVVulkan
{
public:
    SimpleIPTVVulkan();
    ~SimpleIPTVVulkan();
    void Initialize(std::set<std::string> extensions, GLFWwindow* window);
    VkInstance GetVKInstance() const
    {
        return instance;
    }
    VkPhysicalDevice GetPhysicalDevice() const
    {
        return physicalDevice;
    }
    VkDevice GetDevice() const
    {
        return device;
    }
    VkDescriptorPool GetImguiPool() const
    {
        return imguiPool;
    }

    uint32_t GetQueueFamily() const
    {
        return graphicsQueueFamily;
    }
    VkQueue GetQueue() const
    {
        return graphicsQueue;
    }

    const VkFormat* GetSwapchainImageFormat() const
    {
        return &swapchainImageFormat;
    }

    VkRenderPass GetRenderPass() const
    {
        return renderPass;
    }
    pl_renderer GetPlRenderer() const
    {
        return renderer;
    }
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
    void Draw(const ImVec2& windowSize, MpvPlayer* player);
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
    void createVulkanInstance(std::set<std::string> extensions,
        GLFWwindow* window);
    void cleanupVulkan();
    void initSwapchain();
    void initImgui();
    void initImguiFont();
    void resizeSwapchain();
    void updateImguiDrawBuffers();
    void createCustomShader(pl_shader sh, pl_tex texture);
    void initPlCache();
    void destroyPlCache();
    static int debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
private:
    VkInstance instance = VK_NULL_HANDLE;
    VmaAllocator allocator;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = nullptr;
    VkQueue graphicsQueue = nullptr;
    uint32_t graphicsQueueFamily = 0;
    VkFormat swapchainImageFormat;
    VkRenderPass renderPass = nullptr;

    pl_log logger = nullptr;
    pl_vulkan vulkan = nullptr;
    pl_renderer renderer = nullptr;
    pl_swapchain swapchain = nullptr;
    pl_dispatch dispatch = nullptr;
    pl_cache placeboCache = nullptr;
    uint64_t placeboCacheSig = 0;
    std::filesystem::path cacheFile;

    VkDescriptorPool imguiPool;

    GLFWwindow* window = nullptr;
    pl_vertex_attrib attribs_pl[3];
    pl_tex imguiFontTex = nullptr;
    std::unordered_set<pl_tex> customTextures;
    pl_tex playerBarTexture = nullptr;
    pl_tex channelsLogosAtlas = nullptr;

    std::vector<ImDrawVert> imguiDrawVertexes;
    std::vector<ImDrawIdx> imguiDrawIndexes;
    std::vector<ImGuiDrawCommand> imguiDrawCommands;

    int channelsLogoHeight = 0;
};