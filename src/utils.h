#pragma once

#include <filesystem>

class Utils
{
public:
    static std::filesystem::path GetHomeFolder();
    static std::filesystem::path GetAppConfigFolder();
    static void LoadFonts();
    static void disableComputerSleep();
    static void enableComputerSleep();
    static void setComputerSleep(bool flag);

private:
    static void AddFont(const unsigned char* fontData,
                        const unsigned int fontDataSize,
                        float fontSize);
};