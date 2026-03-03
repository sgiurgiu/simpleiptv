#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <filesystem>
#include <memory>

class SettingsRepository : public std::enable_shared_from_this<SettingsRepository>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    SettingsRepository(Key);
    static std::shared_ptr<SettingsRepository> Create();

    int GetWindowWidth(int defaultValue);
    int GetWindowHeight(int defaultValue);
    void SetWindowWidth(int value);
    void SetWindowHeight(int value);
    int GetChannelsWindowWidth(int defaultValue);
    void SetChannelsWindowWidth(int value);
    std::filesystem::path
    GetScreenshotPath(const std::filesystem::path& defaultValue);
    void SetScreenshotPath(const std::filesystem::path& path);
    std::string GetScreenshotFormat(const std::string& defaultValue);
    void SetScreenshotFormat(const std::string& format);
    std::string GetScreenshotFileTemplate(const std::string& defaultValue);
    void SetScreenshotFileTemplate(const std::string& fileTemplate);

private:
    int GetIntValue(const std::string& key, int defaultValue);
    void SetIntValue(const std::string& key, int value);
    std::string GetStringValue(const std::string& key,
                               const std::string& defaultValue);
    void SetStringValue(const std::string& key, const std::string& value);
};