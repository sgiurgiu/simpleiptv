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
            int xstreamServerId);
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

private:
    int id;
    std::string name;
    std::string uri;
    std::string logoUri;
    std::string logo;
    std::string epgChannelUri;
    std::string epgChannelId;
    int xstreamServerId;
};
using ChannelPtr = std::shared_ptr<Channel>;