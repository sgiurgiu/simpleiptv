#include "simpleiptv_vulkan.h"

#include <GLFW/glfw3.h>
#include <libplacebo/colorspace.h>
#include <libplacebo/common.h>
#include <libplacebo/gpu.h>
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

int SimpleIPTVVulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/)
{
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        spdlog::error("Vulkan debug error: {}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        spdlog::warn("Vulkan debug warning: {}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        spdlog::info("Vulkan debug info: {}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        spdlog::debug("Vulkan debug verbose: {}", pCallbackData->pMessage);
        break;
    default:
        spdlog::trace("Vulkan debug unknown: {}", pCallbackData->pMessage);
        break;
    }
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
void SimpleIPTVVulkan::Initialize(const char** extensions,
                                  int extensions_count,
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

    createVulkanInstance(extensions, extensions_count, window);
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

void SimpleIPTVVulkan::UpdateImguiDrawBuffers()
{
    std::lock_guard<std::mutex> _{ imguiRenderMutex };
    auto drawData = ImGui::GetDrawData();
    if (!drawData)
    {
        return;
    }
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

void SimpleIPTVVulkan::drawImgui(pl_swapchain_frame* frame,
                                 const std::vector<ImDrawVert>& vertexes,
                                 const std::vector<ImDrawIdx>& indexes,
                                 const std::vector<ImGuiDrawCommand>& commands)
{
    for (const auto& cmd : commands)
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
        int clipX0 = std::max((int)(pcmd->ClipRect.x), 0);
        int clipY0 = std::max((int)(pcmd->ClipRect.y), 0);
        int clipX1 = std::max((int)(pcmd->ClipRect.z), 0);
        int clipY1 = std::max((int)(pcmd->ClipRect.w), 0);
        if (clipX1 <= clipX0 || clipY1 <= clipY0)
        {
            continue;
        }
        dispatchParams.scissors = { .x0 = clipX0,
                                    .y0 = clipY0,
                                    .x1 = clipX1,
                                    .y1 = clipY1 };
        dispatchParams.vertex_attribs = attribs_pl;
        dispatchParams.num_vertex_attribs = 3;
        dispatchParams.vertex_stride = sizeof(ImDrawVert);
        dispatchParams.vertex_position_idx = 0;
        dispatchParams.vertex_coords = PL_COORDS_ABSOLUTE;
        dispatchParams.vertex_flipped = frame->flipped;
        dispatchParams.vertex_type = PL_PRIM_TRIANGLE_LIST;
        dispatchParams.vertex_count = (int)pcmd->ElemCount;
        dispatchParams.vertex_data =
            vertexes.data() + cmd.vertexOffset + pcmd->VtxOffset;

        dispatchParams.index_data =
            indexes.data() + cmd.indexOffset + pcmd->IdxOffset;
        dispatchParams.index_fmt = PL_INDEX_UINT16;
        dispatchParams.index_offset = 0;

        pl_dispatch_vertex(dispatch, &dispatchParams);
    }
}

void SimpleIPTVVulkan::DrawUI(pl_swapchain_frame* frame)
{
    std::lock_guard<std::mutex> _{ imguiRenderMutex };
    drawImgui(frame, imguiDrawVertexes, imguiDrawIndexes, imguiDrawCommands);
}

void SimpleIPTVVulkan::DrawBackgroundFrame(pl_swapchain_frame* frame)
{
    const float color[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    pl_tex_clear(vulkan->gpu, frame->fbo, color);
}

void SimpleIPTVVulkan::ResizeSwapchain(int width, int height)
{
    pl_swapchain_resize(swapchain, &width, &height);
}

void SimpleIPTVVulkan::createVulkanInstance(const char** extensions,
                                            int extensions_count,
                                            GLFWwindow* window)
{
    this->window = window;
    volkInitialize();
    pl_vk_inst_params vk_inst_params = {};
    vk_inst_params.get_proc_addr = glfwGetInstanceProcAddress;
    vk_inst_params.debug = false;
    vk_inst_params.extensions = extensions;
    vk_inst_params.num_extensions = extensions_count;
    vk_instance = pl_vk_inst_create(logger, &vk_inst_params);
    auto err = glfwCreateWindowSurface(vk_instance->instance, window, nullptr,
                                       &surface);
    CheckError(err);
    pl_vulkan_params vk_params = {};
    vk_params.instance = vk_instance->instance;
    vk_params.get_proc_addr = vk_instance->get_proc_addr;
    vk_params.surface = surface;
    vk_params.allow_software = true;
    vulkan = pl_vulkan_create(logger, &vk_params);

    pl_vulkan_swapchain_params swapchain_params = {};
    swapchain_params.surface = surface;
    swapchain_params.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_params.swapchain_depth = 2;
    swapchain = pl_vulkan_create_swapchain(vulkan, &swapchain_params);
    int w, h;
    glfwGetWindowSize(window, &w, &h);

    pl_swapchain_colorspace_hint(swapchain, nullptr);

    if (!pl_swapchain_resize(swapchain, &w, &h))
    {
        spdlog::error("libplacebo: Failed initializing swapchain");
    }
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

    pl_tex_destroy(vulkan->gpu, &imguiFontTex);
    pl_dispatch_destroy(&dispatch);
    pl_swapchain_destroy(&swapchain);

    pl_vulkan_destroy(&vulkan);
    pl_vk_inst_destroy(&vk_instance);

    volkFinalize();
}
void SimpleIPTVVulkan::WaitForIdle()
{
    pl_gpu_finish(vulkan->gpu);
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
    {
        std::lock_guard<std::mutex> _{ imguiRenderMutex };
        imageData.name = "texture" + std::to_string(customTextures.size());
        customTextures.insert(imageData.tex);
    }

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
    {
        std::lock_guard<std::mutex> _{ imguiRenderMutex };
        playerBarTexture = imageData.tex;
    }

    return imageData;
}
void SimpleIPTVVulkan::DestroyPlayerBarImageData(ImageData& imageData)
{
    DestroyImageData(imageData);
    std::lock_guard<std::mutex> _{ imguiRenderMutex };
    playerBarTexture = nullptr;
}

void SimpleIPTVVulkan::DestroyImageData(ImageData& image)
{
    std::lock_guard<std::mutex> _{ imguiRenderMutex };
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
