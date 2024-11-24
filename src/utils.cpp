#include "utils.h"

#include "fonts/fonts.h"
#include <imgui.h>

#ifdef STV_WINDOWS
#include <shlobj.h>
#include <shlobj_core.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

void Utils::AddFont(const unsigned int* fontData,
                    const unsigned int fontDataSize,
                    float fontSize)
{
    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX =
        fontSize; // Use if you want to make the icon monospaced
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fontData, fontDataSize,
                                                         fontSize);
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
        FontAwesome_compressed_data, FontAwesome_compressed_size, fontSize,
        &config, icon_ranges);
}

void Utils::LoadFonts()
{
    ImGui::GetIO().Fonts->Clear();
    AddFont(Roboto_Regular_ttf_compressed_data,
            Roboto_Regular_ttf_compressed_size, 16.0f);
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
