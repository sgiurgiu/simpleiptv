#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

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

private:
    int GetIntValue(const std::string& key, int defaultValue);
    void SetIntValue(const std::string& key, int value);
};