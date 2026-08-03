#pragma once

#include <memory>
#include <mutex>
#include <optional>
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
    bool IsLogoEmpty() const
    {
        std::lock_guard<std::mutex> _{ logoMutex };
        return logo.empty();
    }
    std::size_t GetLogoSize() const
    {
        std::lock_guard<std::mutex> _{ logoMutex };
        return logo.size();
    }
    /**
     * A snapshot of the encoded logo, taken under the lock.
     *
     * Deliberately a copy: SetLogo() runs on the download/DB thread while
     * decoders read the logo on the network pool or the render thread, so
     * handing out a pointer (or a reference) into the live string would dangle
     * as soon as the assignment reallocates it.
     */
    std::string GetLogoCopy() const
    {
        std::lock_guard<std::mutex> _{ logoMutex };
        return logo;
    }

    void SetLogo(const std::string& logo)
    {
        std::lock_guard<std::mutex> _{ logoMutex };
        this->logo = logo;
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
    bool IsFavourite() const
    {
        return favourite;
    }
    void SetFavourite(bool value)
    {
        favourite = value;
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
    mutable std::mutex logoMutex;
};
using ChannelPtr = std::shared_ptr<Channel>;
