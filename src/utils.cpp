#include "utils.h"

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

#ifdef STV_UNIX
#include <dbus/org.freedesktop.ScreenSaver.xml.h>
#include <sdbus-c++/sdbus-c++.h>
#endif

namespace
{
#ifdef STV_UNIX
static uint32_t screenSaverDBusCookie = 0;

class ScreenSaverProxy final
: public sdbus::ProxyInterfaces<org::freedesktop::ScreenSaver_proxy>
{
public:
    ScreenSaverProxy(sdbus::IConnection &connection,
                     sdbus::ServiceName destination,
                     sdbus::ObjectPath path)
    : ProxyInterfaces(connection, std::move(destination), std::move(path))
    {
        registerProxy();
    }

    ~ScreenSaverProxy()
    {
        unregisterProxy();
    }

    void onActiveChanged(const bool &)
    {
    }
};
#endif
#include "fonts/fonts.h"
} // namespace

void Utils::AddFont(const unsigned char *fontData,
                    const unsigned int fontDataSize,
                    float fontSize)
{
    ImFontConfig fontAwesomeConfig;
    fontAwesomeConfig.MergeMode = true;
    fontAwesomeConfig.GlyphMinAdvanceX =
        fontSize; // Use if you want to make the icon monospaced
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
        FontAwesome_compressed_data, FontAwesome_compressed_size, fontSize,
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

void Utils::disableComputerSleep()
{
    setComputerSleep(false);
}
void Utils::enableComputerSleep()
{
    setComputerSleep(true);
}
void Utils::setComputerSleep(bool flag)
{
#ifdef STV_UNIX
    try
    {
        auto connection = sdbus::createSessionBusConnection();
        sdbus::ServiceName destination{ "org.freedesktop.ScreenSaver" };
        sdbus::ObjectPath objectPath{ "/org/freedesktop/ScreenSaver" };

        auto screenSaverProxy = std::make_unique<ScreenSaverProxy>(
            *connection, std::move(destination), std::move(objectPath));

        if (!flag)
        {
            screenSaverDBusCookie =
                screenSaverProxy->Inhibit("simpleiptv", "playing video");
        }
        else
        {
            screenSaverProxy->UnInhibit(screenSaverDBusCookie);
        }
    }
    catch (const sdbus::Error &er)
    {
        spdlog::error("Error making dbus call to ScreenSaver service: {} - {}",
                      er.getName(), er.getMessage());
    }

#elif defined STV_WIN

    EXECUTION_STATE result;

    if (!flag)
        result = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                                         ES_DISPLAY_REQUIRED);
    else
        result = SetThreadExecutionState(ES_CONTINUOUS);

    if (result == nullptr)
        spdlog::debug("EXECUTION_STATE failed");

#endif
}
