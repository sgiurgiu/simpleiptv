#include "settings_repository.h"

#include "dbconnection_pool.h"
#include <charconv>
#include <soci/soci.h>
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

int SettingsRepository::GetIntValue(const std::string& key, int defaultValue)
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
        else
        {
            return defaultValue;
        }
    }
    else
    {
        return defaultValue;
    }
}

void SettingsRepository::SetIntValue(const std::string& key, int value)
{
    auto session = DatabaseConnections::GetConnection();
    session << "INSERT INTO SETTINGS(KEY,VALUE) VALUES(:KEY,:VAL) ON "
               "CONFLICT(KEY) DO UPDATE SET VALUE=:VAL",
        soci::use(key, "KEY"), soci::use(value, "VAL");
}