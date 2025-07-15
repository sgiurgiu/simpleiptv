#include "stv_utils.h"

#include <cstdint>
#include <imgui.h>
#include <spdlog/spdlog.h>

#ifdef STV_WINDOWS
#include <shlobj.h>
#include <shlobj_core.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
#include "fonts/fonts.h"
} // namespace

void Utils::AddFont(const unsigned char *fontData,
                    const unsigned int fontDataSize,
                    float fontSize)
{
    ImFontConfig fontAwesomeConfig;
    fontAwesomeConfig.MergeMode = true;
    float iconFontSize = fontSize * 2.f / 3.f;
    fontAwesomeConfig.GlyphMinAdvanceX =
        iconFontSize; // Use if you want to make the icon monospaced
    fontAwesomeConfig.PixelSnapH = true;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    // clang-format off
    static const ImWchar font_ranges[] = { 0x0020, 0x052F,
                                           0x1AB0, 0x1ABE,
                                           0x1C80, 0x1C88,
                                           0x1D00, 0x1DFF,
                                           0x1E00, 0x1FFE,
                                           0x2000, 0x2189,
                                           0x2C60, 0x2E44,
                                           0xA640, 0xAB65,
                                           0xFB00, 0xFB06,
                                           0xFE00, 0xFEFF,
                                           0 };
    // clang-format on
    ImFontConfig fontConfig;
    fontConfig.MergeMode = false;
    fontConfig.FontDataOwnedByAtlas = false;

    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        (void *)fontData, fontDataSize, fontSize, &fontConfig, font_ranges);
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
        FontAwesome_compressed_data, FontAwesome_compressed_size, iconFontSize,
        &fontAwesomeConfig, icon_ranges);
}

void Utils::LoadFonts()
{
    ImGui::GetIO().Fonts->Clear();
    AddFont(NotoSans_Regular_ttf, NotoSans_Regular_ttf_len, 16.0f);
}

std::filesystem::path Utils::GetHomeFolder()
{
    std::filesystem::path homePath;
#ifdef STV_WINDOWS
    char homeDirStr[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, homeDirStr)))
    {
        homePath = homeDirStr;
    }
#else
    auto pwd = getpwuid(getuid());
    if (pwd)
    {
        homePath = pwd->pw_dir;
    }
    else
    {
        // try the $HOME environment variable
        homePath = getenv("HOME");
    }
#endif

    if (homePath.empty())
    {
        homePath = "./";
    }
    return homePath;
}
std::filesystem::path Utils::GetAppConfigFolder()
{
    auto homePath = GetHomeFolder();
    std::string relativeConfigFolder;
#ifdef STV_WINDOWS
    relativeConfigFolder = "simpleiptv";
#else
    relativeConfigFolder = ".config/simpleiptv";
#endif

    std::filesystem::path configFolder = homePath / relativeConfigFolder;
    std::filesystem::create_directories(configFolder);
    return configFolder;
}

void Utils::DrawRectangle(uint8_t *img,
                          int width,
                          int height,
                          int channels,
                          int x,
                          int y,
                          int w,
                          int h,
                          unsigned char r,
                          unsigned char g,
                          unsigned char b)
{
    for (int j = y; j < y + h; ++j)
    {
        for (int i = x; i < x + w; ++i)
        {
            if (i >= 0 && i < width && j >= 0 && j < height)
            {
                int pixel_offset = (j * width + i) * channels;
                img[pixel_offset] = r;
                if (channels > 1)
                {
                    img[pixel_offset + 1] = g;
                    img[pixel_offset + 2] = b;
                }
                if (channels > 3)
                {
                    img[pixel_offset + 3] = 255;
                }
            }
        }
    }
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>
Utils::GetContrastColor(uint8_t *img, int width, int height, int channels)
{
    if (channels != 4)
        return std::make_tuple(0, 0, 0, 255);

    int64_t total_r = 0;
    int64_t total_b = 0;
    int64_t total_g = 0;
    int64_t foreground_pixel_count = 0;

    // 1. First pass: Calculate the average color of the foreground.
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            int pixel_offset = (j * width + i) * channels;
            uint8_t alpha = img[pixel_offset + 3];
            if (alpha != 0)
            {
                total_r += img[pixel_offset + 0];
                total_g += img[pixel_offset + 1];
                total_b += img[pixel_offset + 2];
                foreground_pixel_count++;
            }
        }
    }

    uint8_t bg_r = 0;
    uint8_t bg_g = 0;
    uint8_t bg_b = 0;

    // 2. Determine the best contrast color.
    if (foreground_pixel_count > 0)
    {
        double avg_r = static_cast<double>(total_r) / foreground_pixel_count;
        double avg_g = static_cast<double>(total_g) / foreground_pixel_count;
        double avg_b = static_cast<double>(total_b) / foreground_pixel_count;

        // A simple way to find a complementary color is to invert the color
        // against the sum of its min and max components.
        unsigned char min_c = std::min({ avg_r, avg_g, avg_b });
        unsigned char max_c = std::max({ avg_r, avg_g, avg_b });
        unsigned int sum_c = min_c + max_c;

        bg_r = static_cast<unsigned char>(sum_c - avg_r);
        bg_g = static_cast<unsigned char>(sum_c - avg_g);
        bg_b = static_cast<unsigned char>(sum_c - avg_b);
    }
    else
    {
        // The image is fully transparent, so we'll use a white background by
        // default.
        bg_r = 255;
        bg_g = 255;
        bg_b = 255;
    }

    return std::make_tuple(bg_r, bg_g, bg_b, 255);
}

void Utils::EnhanceLogo(uint8_t *img, int width, int height, int channels)
{
    if (channels != 4)
        return;

    int64_t total_r = 0;
    int64_t total_b = 0;
    int64_t total_g = 0;
    int64_t foreground_pixel_count = 0;

    // 1. First pass: Calculate the average color of the foreground.
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            int pixel_offset = (j * width + i) * channels;
            uint8_t alpha = img[pixel_offset + 3];
            if (alpha != 0)
            {
                total_r += img[pixel_offset + 0];
                total_g += img[pixel_offset + 1];
                total_b += img[pixel_offset + 2];
                foreground_pixel_count++;
            }
        }
    }

    uint8_t bg_r = 0;
    uint8_t bg_g = 0;
    uint8_t bg_b = 0;

    // 2. Determine the best contrast color.
    if (foreground_pixel_count > 0)
    {
        double avg_r = static_cast<double>(total_r) / foreground_pixel_count;
        double avg_g = static_cast<double>(total_g) / foreground_pixel_count;
        double avg_b = static_cast<double>(total_b) / foreground_pixel_count;

        // A simple way to find a complementary color is to invert the color
        // against the sum of its min and max components.
        unsigned char min_c = std::min({ avg_r, avg_g, avg_b });
        unsigned char max_c = std::max({ avg_r, avg_g, avg_b });
        unsigned int sum_c = min_c + max_c;

        bg_r = static_cast<unsigned char>(sum_c - avg_r);
        bg_g = static_cast<unsigned char>(sum_c - avg_g);
        bg_b = static_cast<unsigned char>(sum_c - avg_b);
    }
    else
    {
        // The image is fully transparent, so we'll use a white background by
        // default.
        bg_r = 255;
        bg_g = 255;
        bg_b = 255;
    }

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            int pixel_offset = (j * width + i) * channels;
            uint8_t alpha = img[pixel_offset + 3];
            if (alpha == 0)
            {
                img[pixel_offset + 0] = bg_r;
                img[pixel_offset + 1] = bg_g;
                img[pixel_offset + 2] = bg_b;
                img[pixel_offset + 3] = 255; // Make the new background opaque.
            }
        }
    }
}