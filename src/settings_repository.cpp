#include "settings_repository.h"

#include "dbconnection_pool.h"
#include <charconv>
#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <system_error>

SettingsRepository::SettingsRepository(Key)
{
}
std::shared_ptr<SettingsRepository> SettingsRepository::Create()
{
    return std::make_shared<SettingsRepository>(Key{});
}

int SettingsRepository::GetWindowWidth(int defaultValue)
{
    return GetIntValue("WINDOW_WIDTH", defaultValue);
}
int SettingsRepository::GetWindowHeight(int defaultValue)
{
    return GetIntValue("WINDOW_HEIGHT", defaultValue);
}
void SettingsRepository::SetWindowWidth(int value)
{
    SetIntValue("WINDOW_WIDTH", value);
}
void SettingsRepository::SetWindowHeight(int value)
{
    SetIntValue("WINDOW_HEIGHT", value);
}

int SettingsRepository::GetChannelsWindowWidth(int defaultValue)
{
    return GetIntValue("CHANNELS_WINDOW_WIDTH", defaultValue);
}
void SettingsRepository::SetChannelsWindowWidth(int value)
{
    SetIntValue("CHANNELS_WINDOW_WIDTH", value);
}

std::filesystem::path
SettingsRepository::GetScreenshotPath(const std::filesystem::path& defaultValue)
{
    return GetStringValue("SCREENSHOT_PATH", defaultValue.string());
}
void SettingsRepository::SetScreenshotPath(const std::filesystem::path& path)
{
    SetStringValue("SCREENSHOT_PATH", path.string());
}

std::string SettingsRepository::GetScreenshotFormat(const std::string& defaultValue)
{
    return GetStringValue("SCREENSHOT_FORMAT", defaultValue);
}
void SettingsRepository::SetScreenshotFormat(const std::string& format)
{
    SetStringValue("SCREENSHOT_FORMAT", format);
}

std::string
SettingsRepository::GetScreenshotFileTemplate(const std::string& defaultValue)
{
    return GetStringValue("SCREENSHOT_FILE_TEMPLATE", defaultValue);
}
void SettingsRepository::SetScreenshotFileTemplate(const std::string& fileTemplate)
{
    SetStringValue("SCREENSHOT_FILE_TEMPLATE", fileTemplate);
}

std::string SettingsRepository::GetStringValue(const std::string& key,
                                               const std::string& defaultValue)
{
    try
    {
        auto session = DatabaseConnections::GetConnection();
        std::string valueStr;
        soci::indicator ind;
        session << "SELECT VALUE FROM SETTINGS WHERE KEY=:KEY", soci::use(key),
            soci::into(valueStr, ind);
        if (ind == soci::i_ok)
        {
            return valueStr;
        }
        return defaultValue;
    }
    catch (const soci::soci_error& ex)
    {
        spdlog::error("Cannot load string setting '{}': {}", key, ex.what());
        return defaultValue;
    }
}
void SettingsRepository::SetStringValue(const std::string& key,
                                        const std::string& value)
{
    try
    {
        auto session = DatabaseConnections::GetConnection();
        session << "INSERT INTO SETTINGS(KEY,VALUE) VALUES(:KEY,:VAL) ON "
                   "CONFLICT(KEY) DO UPDATE SET VALUE=:VAL",
            soci::use(key, "KEY"), soci::use(value, "VAL");
    }
    catch (const soci::soci_error& ex)
    {
        spdlog::error("Cannot save string setting '{}': {}", key, ex.what());
    }
}

int SettingsRepository::GetIntValue(const std::string& key, int defaultValue)
{
    try
    {
        auto session = DatabaseConnections::GetConnection();
        std::string valueStr;
        soci::indicator ind;
        session << "SELECT VALUE FROM SETTINGS WHERE KEY=:KEY", soci::use(key),
            soci::into(valueStr, ind);
        if (ind == soci::i_ok)
        {
            int value;
            auto [ptr, ec] = std::from_chars(
                valueStr.data(), valueStr.data() + valueStr.size(), value);
            if (ec == std::errc())
            {
                return value;
            }
        }
        return defaultValue;
    }
    catch (const soci::soci_error& ex)
    {
        spdlog::error("Cannot load int setting '{}': {}", key, ex.what());
        return defaultValue;
    }
}

void SettingsRepository::SetIntValue(const std::string& key, int value)
{
    try
    {
        auto session = DatabaseConnections::GetConnection();
        session << "INSERT INTO SETTINGS(KEY,VALUE) VALUES(:KEY,:VAL) ON "
                   "CONFLICT(KEY) DO UPDATE SET VALUE=:VAL",
            soci::use(key, "KEY"), soci::use(value, "VAL");
    }
    catch (const soci::soci_error& ex)
    {
        spdlog::error("Cannot save int setting '{}': {}", key, ex.what());
    }
}