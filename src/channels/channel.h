#pragma once

#include <memory>
#include <string>

class Channel
{
public:
    Channel(int id,
            std::string name,
            std::string uri,
            std::string logoUri,
            std::string logo,
            std::string epgChannelUri,
            std::string epgChannelId,
            int xstreamServerId,
            bool favourite,
            std::optional<int> parentId);
    Channel(const Channel&) = default;
    Channel(Channel&&) = default;
    Channel& operator=(const Channel&) = default;
    Channel& operator=(Channel&&) = default;

    int GetId() const
    {
        return id;
    }
    const std::string& GetName() const
    {
        return name;
    }
    const std::string& GetUri() const
    {
        return uri;
    }
    const std::string& GetLogoUri() const
    {
        return logoUri;
    }
    const std::string& GetLogo() const
    {
        return logo;
    }
    const std::string& GetEPGChannelUri() const
    {
        return epgChannelUri;
    }
    const std::string& GetEPGChannelId() const
    {
        return epgChannelId;
    }
    int GetXStreamServerId() const
    {
        return xstreamServerId;
    }
    std::optional<int> GetParentId() const
    {
        return parentId;
    }
    int IsFavourite() const
    {
        return favourite;
    }

private:
    int id;
    std::string name;
    std::string uri;
    std::string logoUri;
    std::string logo;
    std::string epgChannelUri;
    std::string epgChannelId;
    int xstreamServerId;
    bool favourite = false;
    std::optional<int> parentId;
};
using ChannelPtr = std::shared_ptr<Channel>;