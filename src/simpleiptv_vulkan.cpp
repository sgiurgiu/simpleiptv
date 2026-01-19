#include "simpleiptv_vulkan.h"

#include <libplacebo/common.h>

#include <libplacebo/colorspace.h>
#include <libplacebo/renderer.h>
#include <libplacebo/swapchain.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <libplacebo/shaders.h>
#include <libplacebo/shaders/sampling.h>
#include <libplacebo/vulkan.h>

#include <algorithm>
#include <fstream>
#include <imgui_impl_vulkan.h>
#include <spdlog/spdlog.h>
#include <vulkan/vk_enum_string_helper.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef STV_DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

#define IMAGE_POOL_SIZE (2)

#include "stv_utils.h"
#include "mpvplayer.h"

namespace
{
void pllog_callback(void*, enum pl_log_level level, const char* msg)
{
    switch (level)
    {
    case PL_LOG_FATAL:
        spdlog::critical("Placebo fatal: {}", msg);
        break;
    case PL_LOG_ERR:
        spdlog::error("Placebo error: {}", msg);
        break;
    case PL_LOG_WARN:
        spdlog::warn("Placebo warn: {}", msg);
        break;
    case PL_LOG_INFO:
        spdlog::info("Placebo info: {}", msg);
        break;
    case PL_LOG_DEBUG:
        spdlog::debug("Placebo debug: {}", msg);
        break;
    case PL_LOG_TRACE:
        spdlog::trace("Placebo trace: {}", msg);
        break;
    default:
        break;
    };
}
} // namespace

int SimpleIPTVVulkan::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    spdlog::debug("Vulkan debug: {}", pCallbackData->pMessage);
    return VK_TRUE;
}

SimpleIPTVVulkan::SimpleIPTVVulkan()
{
}

SimpleIPTVVulkan::~SimpleIPTVVulkan()
{
    WaitForIdle();
    cleanupVulkan();
    destroyPlCache();
}
void SimpleIPTVVulkan::Initialize(std::set<std::string> extensions,
    GLFWwindow* window)
{
    pl_log_params logParams = { .log_cb = pllog_callback,
                                .log_priv = nullptr,
#ifdef STV_DEBUG
                                .log_level = PL_LOG_DEBUG
#else
                                .log_level = PL_LOG_INFO
#endif
    };
    logger = pl_log_create(PL_API_VER, &logParams);

    createVulkanInstance(std::move(extensions), window);
    initSwapchain();
    // initImgui();
    initImguiFont();
    initPlCache();
    dispatch = pl_dispatch_create(logger, vulkan->gpu);

    attribs_pl[0] = { .name = "pos",
                      .fmt = pl_find_vertex_fmt(vulkan->gpu, PL_FMT_FLOAT, 2),
                      .offset = offsetof(ImDrawVert, pos),
                      .location = 0 };
    attribs_pl[1] = { .name = "coord",
                      .fmt = pl_find_vertex_fmt(vulkan->gpu, PL_FMT_FLOAT, 2),
                      .offset = offsetof(ImDrawVert, uv),
                      .location = 0 };
    attribs_pl[2] = { .name = "vcolor",
                      .fmt = pl_find_named_fmt(vulkan->gpu, "rgba8"),
                      .offset = offsetof(ImDrawVert, col),
                      .location = 0 };
}

void SimpleIPTVVulkan::createCustomShader(pl_shader sh, pl_tex texture)
{
    pl_shader_desc shader_desc[1] = {};
    shader_desc[0].binding = {};
    shader_desc[0].binding.object = texture;
    shader_desc[0].binding.sample_mode = PL_TEX_SAMPLE_LINEAR;
    shader_desc[0].desc = {};
    shader_desc[0].desc.type = PL_DESC_SAMPLED_TEX;
    shader_desc[0].desc.name = "tex";

    pl_custom_shader custom_shader = {};
    custom_shader.description = "imgui UI";
    custom_shader.output = PL_SHADER_SIG_COLOR;
    custom_shader.input = PL_SHADER_SIG_NONE;
    if (texture)
    {
        custom_shader.body = R"(
                vec4 fontColor = texture(tex, coord);
                color =  vcolor * fontColor;
                )";
    }
    else
    {
        custom_shader.body = R"(
                color =  vcolor;
                )";
    }
    custom_shader.num_descriptors = 1;
    custom_shader.descriptors = shader_desc;

    pl_shader_custom(sh, &custom_shader);
}

void SimpleIPTVVulkan::updateImguiDrawBuffers()
{
    auto drawData = ImGui::GetDrawData();
    if (imguiDrawVertexes.size() != (size_t)drawData->TotalVtxCount)
    {
        imguiDrawVertexes.resize((size_t)drawData->TotalVtxCount);
    }
    if (imguiDrawIndexes.size() != (size_t)drawData->TotalIdxCount)
    {
        imguiDrawIndexes.resize((size_t)drawData->TotalIdxCount);
    }

    imguiDrawCommands.clear();
    int vertexOffset = 0;
    int indexOffset = 0;
    ImDrawVert* vtxDst = imguiDrawVertexes.data();
    ImDrawIdx* idxDst = imguiDrawIndexes.data();
    for (int32_t i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[i];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data,
               (size_t)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data,
               (size_t)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;

        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            imguiDrawCommands.push_back(
                { pcmd->GetTexID(), pcmd, vertexOffset, indexOffset });
        }
        vertexOffset += cmd_list->VtxBuffer.Size;
        indexOffset += cmd_list->IdxBuffer.Size;
    }
}

void SimpleIPTVVulkan::drawImgui(pl_swapchain_frame* frame)
{
    for (const auto& cmd : imguiDrawCommands)
    {
        ImTextureID textureId = cmd.textureId;

        pl_tex currentTexture = imguiFontTex;
        if ((void*)textureId == imguiFontTex)
        {
            currentTexture = imguiFontTex;
        }
        else if ((void*)textureId == playerBarTexture)
        {
            currentTexture = playerBarTexture;
        }
        else if (customTextures.contains((pl_tex)textureId))
        {
            currentTexture = (pl_tex)textureId;
        }
        else
        {
        }

        const ImDrawCmd* pcmd = cmd.pcmd;
        pl_shader sh = pl_dispatch_begin(dispatch);
        createCustomShader(sh, currentTexture);

        bool is_srgb = frame->color_space.primaries == PL_COLOR_PRIM_BT_709 &&
                       frame->color_space.transfer == PL_COLOR_TRC_SRGB;

        struct pl_color_repr repr = pl_color_repr_rgb;
        pl_color_map_args map_args = {};
        map_args.src = frame->color_space;
        map_args.dst = is_srgb ? pl_color_space_srgb : frame->color_space;
        pl_shader_color_map_ex(sh, NULL, &map_args);
        pl_shader_encode_color(sh, &repr);

        pl_dispatch_vertex_params dispatchParams = {};
        dispatchParams.shader = &sh;
        dispatchParams.target = frame->fbo;
        dispatchParams.blend_params = &pl_alpha_overlay;
        dispatchParams.scissors = {
            .x0 = std::max((int32_t)(pcmd->ClipRect.x), 0),
            .y0 = std::max((int32_t)(pcmd->ClipRect.y), 0),
            .x1 = std::max((int)(pcmd->ClipRect.z), 0),
            .y1 = std::max((int)(pcmd->ClipRect.w), 0)
        };
        dispatchParams.vertex_attribs = attribs_pl;
        dispatchParams.num_vertex_attribs = 3;
        dispatchParams.vertex_stride = sizeof(ImDrawVert);
        dispatchParams.vertex_position_idx = 0;
        dispatchParams.vertex_coords = PL_COORDS_ABSOLUTE;
        dispatchParams.vertex_flipped = frame->flipped;
        dispatchParams.vertex_type = PL_PRIM_TRIANGLE_LIST;
        dispatchParams.vertex_count = (int)pcmd->ElemCount;
        dispatchParams.vertex_data =
            imguiDrawVertexes.data() + cmd.vertexOffset + pcmd->VtxOffset;

        dispatchParams.index_data =
            imguiDrawIndexes.data() + cmd.indexOffset + pcmd->IdxOffset;
        dispatchParams.index_fmt = PL_INDEX_UINT16;
        dispatchParams.index_offset = 0;

        pl_dispatch_vertex(dispatch, &dispatchParams);
    }
}

void SimpleIPTVVulkan::Draw(const ImVec2& windowSize, MpvPlayer* player)
{
    pl_swapchain_frame frame = {};
    if (!pl_swapchain_start_frame(swapchain, &frame) ) {
        spdlog::error("[render] failed to get swapchain frame!");
        return;
    }

    if (player && player->GetPlayerState() == PlayerState::PLAYING)
    {
        player->Render(&frame, windowSize);
    }
    else
    {
        pl_tex_clear(vulkan->gpu, frame.fbo, (float[4]){0.5f, 0.5f, 0.5f, 1.0f});
    }
    updateImguiDrawBuffers();
    drawImgui(&frame);

    pl_gpu_flush(vulkan->gpu);
    pl_swapchain_submit_frame(swapchain);
    pl_swapchain_swap_buffers(swapchain);
}

void SimpleIPTVVulkan::ResizeSwapchain(int width, int height)
{
    WaitForIdle(); // Not sure if this is needed ???

    pl_swapchain_resize(swapchain, &width, &height);
}

void SimpleIPTVVulkan::createVulkanInstance(std::set<std::string> extensions,
    GLFWwindow* window)
{
    this->window = window;
    volkInitialize();


    std::vector<const char*> requiredExtensions;
#ifdef STV_DEBUG
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    for (const auto& ext : extensions)
    {
        requiredExtensions.push_back(ext.c_str());
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Simple IPTV",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Simple IPTV",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    }
    std::vector<const char*> layers = {
#ifdef STV_DEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };
    VkInstanceCreateInfo ici{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = uint32_t(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = uint32_t(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    CheckError(vkCreateInstance(&ici, nullptr, &instance));

    volkLoadInstance(instance);

#ifdef STV_DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)SimpleIPTVVulkan::debugCallback;
    debugCreateInfo.pUserData = nullptr;
    CheckError(vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger));
#endif

    glfwCreateWindowSurface(instance, window, nullptr, &surface);
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        
        for (auto dev : devices) {
            // Check queue families + surface support
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(dev, &properties);
            if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && properties.apiVersion >= VK_API_VERSION_1_2) {
                physicalDevice = dev;
                break;
            }
        }
    }
    if(physicalDevice == VK_NULL_HANDLE) {
        spdlog::error("No suitable physical device found");
        return;
    }

    VkPhysicalDeviceVulkan13Features supported13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
    };
    
    VkPhysicalDeviceVulkan12Features supported12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr
    };
    
    VkPhysicalDeviceFeatures2 supported{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr
    };
    {
        void* tail = nullptr;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        uint32_t apiVersion = properties.apiVersion;
        if (apiVersion >= VK_API_VERSION_1_2) {
            supported12.pNext = tail;
            tail = &supported12;
        }

        if (apiVersion >= VK_API_VERSION_1_3) {
            supported13.pNext = tail;
            tail = &supported13;
        }

        supported.pNext = tail;
    }
    
    vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
    
    VkPhysicalDeviceVulkan13Features enabled13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
    };
    
    VkPhysicalDeviceVulkan12Features enabled12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &enabled13
    };
    
    VkPhysicalDeviceFeatures2 enabled{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &enabled12
    };    
    enabled12.timelineSemaphore =    supported12.timelineSemaphore;
    enabled12.bufferDeviceAddress =        supported12.bufferDeviceAddress;
    enabled12.descriptorIndexing =        supported12.descriptorIndexing;
    enabled12.hostQueryReset =        supported12.hostQueryReset;
    enabled13.synchronization2 =        supported13.synchronization2;
    
    if (!enabled12.timelineSemaphore) {
        throw std::runtime_error("Timeline semaphores not supported");
    }
    

    {
        graphicsQueueFamily = UINT32_MAX;
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qCount, qProps.data());

        for (uint32_t i = 0; i < qCount; i++) {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentSupported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevice, i, surface, &presentSupported);
                if (presentSupported) {
                    graphicsQueueFamily = i;
                    break;
                }
            }
        }
    }

    if (graphicsQueueFamily == UINT32_MAX)
    {
        spdlog::error("No graphics queue family found");
        return;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = graphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &priority
    };

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        //VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME   
    };

    VkDeviceCreateInfo dci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled,              
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &qci,
        .enabledExtensionCount = uint32_t(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = nullptr     
    };
    CheckError(vkCreateDevice(physicalDevice, &dci, nullptr, &device));
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    volkLoadDevice(device);

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = this->physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    VmaVulkanFunctions functions = {};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &functions;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    uint32_t propCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propCount,
                                         nullptr);
    VkExtensionProperties* properties = new VkExtensionProperties[propCount];
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propCount,
                                         properties);
    char** pl_extensions = new char*[propCount];
    for (uint32_t i = 0; i < propCount; i++)
    {
        pl_extensions[i] = properties[i].extensionName;
    }

    pl_vulkan_import_params import_params = {};
    import_params.instance = instance;
    import_params.phys_device = physicalDevice;
    import_params.device = device;
    import_params.queue_graphics.index = graphicsQueueFamily;
    import_params.queue_graphics.count = 1;
    import_params.features = &enabled;
    import_params.get_proc_addr = vkGetInstanceProcAddr;
    import_params.num_extensions = (int)propCount;
    import_params.extensions = pl_extensions;

    vulkan = pl_vulkan_import(logger, &import_params);

    delete[] properties;
    delete[] pl_extensions;

    renderer = pl_renderer_create(logger, vulkan->gpu);
}

void SimpleIPTVVulkan::initSwapchain()
{
    pl_vulkan_swapchain_params sw_params = {};
    sw_params.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    sw_params.surface = surface;
    sw_params.swapchain_depth = 3;
    sw_params.allow_suboptimal = true;
    sw_params.disable_10bit_sdr = true;

    swapchain = pl_vulkan_create_swapchain(vulkan, &sw_params);

    int w, h;
    glfwGetWindowSize(window, &w, &h);

    pl_swapchain_colorspace_hint(swapchain, nullptr);

    if (!pl_swapchain_resize(swapchain, &w, &h))
    {
        spdlog::error("libplacebo: Failed initializing swapchain");
    }
    VkFormat vk_format = VK_FORMAT_UNDEFINED;
    {
        struct pl_tex_params tex_params = {};
        tex_params.w = w;
        tex_params.h = h;
        tex_params.format = pl_find_named_fmt(vulkan->gpu, "rgba8");
        tex_params.sampleable = true;
        tex_params.renderable = true;
        pl_tex imguiImage = nullptr;
        pl_tex_recreate(vulkan->gpu, &imguiImage, &tex_params);

        pl_vulkan_unwrap(vulkan->gpu, imguiImage, &vk_format, nullptr);
        swapchainImageFormat = vk_format;
        pl_tex_destroy(vulkan->gpu, &imguiImage);
    }

    /*{
        VkAttachmentDescription attachment = {};
        attachment.format = vk_format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        auto err = vkCreateRenderPass(
            device, &info, allocator->GetAllocationCallbacks(), &renderPass);
        SimpleIPTVVulkan::CheckError(err);
    }*/
}

void SimpleIPTVVulkan::initImgui()
{
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMAGE_POOL_SIZE },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;

    auto err = vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool);
    CheckError(err);
}

void SimpleIPTVVulkan::initImguiFont()
{
    ImGuiIO& io = ImGui::GetIO();
    // Create font texture
    unsigned char* fontData;
    int texWidth, texHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);

    pl_tex_params tparams = {};
    tparams.blit_dst = true;
    tparams.w = texWidth;
    tparams.h = texHeight;
    tparams.debug_tag = PL_DEBUG_TAG;
    tparams.host_writable = true;
    tparams.initial_data = fontData;
    tparams.sampleable = true;
    tparams.renderable = true;
    tparams.format = pl_find_named_fmt(vulkan->gpu, "rgba8");

    imguiFontTex = pl_tex_create(vulkan->gpu, &tparams);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(imguiFontTex));
}

void SimpleIPTVVulkan::CheckError(VkResult err)
{
    if (err == 0)
        return;
    spdlog::error("[vulkan] Error: VkResult = {}, message = {}", (int)err,
                  string_VkResult(err));
    if (err < 0)
        throw std::runtime_error("Fatal error");
}

void SimpleIPTVVulkan::cleanupVulkan()
{
    vkDeviceWaitIdle(device);
    //  vkDestroyDescriptorPool(device, imguiPool, nullptr);
    //    vkDestroyRenderPass(device, renderPass, nullptr);

    pl_tex_destroy(vulkan->gpu, &imguiFontTex);
    pl_dispatch_destroy(&dispatch);
    pl_swapchain_destroy(&swapchain);

    pl_renderer_destroy(&renderer);
    pl_vulkan_destroy(&vulkan);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
#ifdef STV_DEBUG
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
    vkDestroyInstance(instance, nullptr);

    volkFinalize();
}
void SimpleIPTVVulkan::WaitForIdle()
{
    auto err = vkDeviceWaitIdle(device);
    CheckError(err);
}

ImageData SimpleIPTVVulkan::CreateImageData(int width,
                                            int height,
                                            int channels,
                                            uint8_t* data)
{
    /**
1. Create and Populate the Atlas

First, decide on the size of the atlas and layout the textures efficiently.
Example Code to Generate the Atlas:

#include <vector>
#include <algorithm>

// Example texture data
struct Texture {
    unsigned char* data;
    int width;
    int height;
};

std::vector<Texture> textures;  // Fill this with your textures

// Find the total atlas dimensions (simple row packing for now)
int atlasWidth = 0, atlasHeight = 0;
for (const auto& tex : textures) {
    atlasWidth = std::max(atlasWidth, tex.width);
    atlasHeight += tex.height;
}

// Allocate memory for the atlas
std::vector<unsigned char> atlasData(atlasWidth * atlasHeight * 4, 0);

// Pack the textures row by row
int yOffset = 0;
for (const auto& tex : textures) {
    for (int y = 0; y < tex.height; y++) {
        memcpy(&atlasData[(yOffset + y) * atlasWidth * 4],
               &tex.data[y * tex.width * 4],
               tex.width * 4);
    }
    yOffset += tex.height;
}

2. Upload the Atlas to Libplacebo

pl_tex_params atlasParams = {};
atlasParams.format = pl_find_named_fmt(vulkan->gpu, "rgba8");
atlasParams.sampleable = true;
atlasParams.w = atlasWidth;
atlasParams.h = atlasHeight;
atlasParams.initial_data = atlasData.data();

pl_tex* atlasTexture = pl_tex_create(vulkan->gpu, &atlasParams);

3. Update ImGui UV Coordinates

Store texture regions for each texture in the atlas:

struct TextureUV {
    ImVec2 uv0;  // Bottom-left corner
    ImVec2 uv1;  // Top-right corner
};
std::vector<TextureUV> textureUVs;

yOffset = 0;
for (const auto& tex : textures) {
    TextureUV uv = {
        ImVec2(0.0f, (float)yOffset / atlasHeight),
        ImVec2((float)tex.width / atlasWidth, (float)(yOffset + tex.height) /
atlasHeight)
    };
    textureUVs.push_back(uv);
    yOffset += tex.height;
}

4. Render with ImGui

Use ImGui::Image or ImGui::ImageButton with the atlas texture:

for (int i = 0; i < textureUVs.size(); i++) {
    ImGui::Image((void*)atlasTexture, ImVec2(100, 100), textureUVs[i].uv0,
textureUVs[i].uv1);
}

5. Optimizations

    Texture Packing: Use a texture packing algorithm like MaxRects or Guillotine
to efficiently pack textures. Power of Two Textures: Keep the atlas dimensions
as powers of two for better GPU performance. Mipmaps: Consider generating
mipmaps for smoother downscaling.
     *
     */
    ImageData imageData = {};
    pl_tex_params tparams = {};
    tparams.blit_dst = true;
    tparams.w = width;
    tparams.h = height;
    tparams.debug_tag = PL_DEBUG_TAG;
    tparams.initial_data = data;
    tparams.sampleable = true;
    tparams.renderable = true;
    tparams.format =
        pl_find_named_fmt(vulkan->gpu, (channels == 4) ? "rgba8" : "rgb8");

    if (channelsLogoHeight != height)
    {
        spdlog::error("channelsLogoHeight is  {} and height is {}",
                      channelsLogoHeight, height);
        channelsLogoHeight = height;
    }
    // pl_tex_recreate();
    imageData.tex = pl_tex_create(vulkan->gpu, &tparams);
    imageData.name = "texture" + std::to_string(customTextures.size());
    customTextures.insert(imageData.tex);

    return imageData;
}

ImageData SimpleIPTVVulkan::CreatePlayerBarImageData(int width,
                                                     int height,
                                                     int channels,
                                                     uint8_t* data)
{
    ImageData imageData;
    pl_tex_params tparams = {};
    tparams.blit_dst = false;
    tparams.w = width;
    tparams.h = height;
    tparams.debug_tag = PL_DEBUG_TAG;
    tparams.initial_data = data;
    tparams.sampleable = true;
    tparams.renderable = false;
    if (channels == 4)
    {
        tparams.format = pl_find_named_fmt(vulkan->gpu, "rgba8");
    }
    else
    {
        tparams.format = pl_find_named_fmt(vulkan->gpu, "rgb8");
    }

    imageData.tex = pl_tex_create(vulkan->gpu, &tparams);
    playerBarTexture = imageData.tex;

    return imageData;
}
void SimpleIPTVVulkan::DestroyPlayerBarImageData(ImageData& imageData)
{
    DestroyImageData(imageData);
    playerBarTexture = nullptr;
}

void SimpleIPTVVulkan::DestroyImageData(ImageData& image)
{
    customTextures.erase(image.tex);
    pl_tex_destroy(vulkan->gpu, &image.tex);
}

void SimpleIPTVVulkan::initPlCache()
{
    auto appConfigFolder = Utils::GetAppConfigFolder();
    cacheFile = appConfigFolder / "simpleiptv.cache";
    pl_cache_params params = {};
    params.log = logger;
    params.max_total_size = 50 << 20; // 50 MB

    placeboCache = pl_cache_create(&params);
    pl_gpu_set_cache(vulkan->gpu, placeboCache);
    if (std::filesystem::exists(cacheFile))
    {
        std::ifstream file(cacheFile, std::ios::binary);
        if (file.good())
        {
            pl_cache_load_ex(
                placeboCache,
                [](void* priv, size_t size, void* ptr) -> bool
                {
                    std::ifstream* in = reinterpret_cast<std::ifstream*>(priv);
                    in->read((char*)ptr, (std::streamsize)size);
                    return in->good();
                },
                &file);
            placeboCacheSig = pl_cache_signature(placeboCache);
        }
    }
}
void SimpleIPTVVulkan::destroyPlCache()
{
    if (placeboCache)
    {
        if (placeboCacheSig != pl_cache_signature(placeboCache))
        {
            std::ofstream out(cacheFile, std::ios::binary | std::ios::trunc);
            if (out.good())
            {
                pl_cache_save_ex(
                    placeboCache,
                    [](void* priv, size_t size, const void* ptr)
                    {
                        std::ofstream* out =
                            reinterpret_cast<std::ofstream*>(priv);
                        out->write((const char*)ptr, (std::streamsize)size);
                    },
                    &out);
            }
        }
        pl_cache_destroy(&placeboCache);
    }
}
