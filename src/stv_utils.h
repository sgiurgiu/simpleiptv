#pragma once

#include <cstdint>
#include <filesystem>
#include <tuple>

class Utils
{
public:
    static std::filesystem::path GetHomeFolder();
    static std::filesystem::path GetAppConfigFolder();
    static void LoadFonts();
    static void DrawRectangle(uint8_t* img,
                              int width,
                              int height,
                              int channels,
                              int x,
                              int y,
                              int w,
                              int h,
                              unsigned char r,
                              unsigned char g,
                              unsigned char b);

    /**
     * @brief Replaces transparent pixels with a high-contrast background color.
     *
     * This function analyzes the foreground pixels of an RGBA image to
     * determine a contrasting background color (either black or white) and
     * applies it to all fully transparent pixels.
     *
     * @param img Pointer to the image data buffer (RGBA format).
     * @param width The width of the image in pixels.
     * @param height The height of the image in pixels.
     * @param channels The number of channels in the image (must be 4).
     */
    static void EnhanceLogo(uint8_t* img, int width, int height, int channels);

    static std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>
    GetContrastColor(uint8_t* img, int width, int height, int channels);

private:
    static void AddFont(const unsigned char* fontData,
                        const unsigned int fontDataSize,
                        float fontSize);
};