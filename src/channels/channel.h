#pragma once

#include <memory>
#include <string>

class Channel
{
public:
    Channel(int id, std::string name);
    Channel(const Channel&) = default;
    Channel(Channel&&) = default;
    Channel& operator=(const Channel&) = default;
    Channel& operator=(Channel&&) = default;

    int GetId() const
    {
        return id;
    }
    std::string GetName() const
    {
        return name;
    }

private:
    int id;
    std::string name;
};
using ChannelPtr = std::shared_ptr<Channel>;