#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "channel.h"

class ChannelsGroup;
using ChannelsGroupPtr = std::shared_ptr<ChannelsGroup>;

class ChannelsGroup
{
public:
    ChannelsGroup(int id, std::string name, std::optional<int> parentId);
    virtual ~ChannelsGroup() = default;
    ChannelsGroup(const ChannelsGroup&) = default;
    ChannelsGroup(ChannelsGroup&&) = default;
    ChannelsGroup& operator=(const ChannelsGroup&) = default;
    ChannelsGroup& operator=(ChannelsGroup&&) = default;

    void AddChannelGroups(std::vector<ChannelsGroupPtr> groups);
    void AddChannelGroup(ChannelsGroupPtr group);
    void AddChannels(std::vector<ChannelPtr> channels);
    void AddChannel(ChannelPtr channel);
    void RemoveChannelGroup(int id);
    void RemoveChannel(int id);

    int GetId() const
    {
        return id;
    }
    const std::string& GetName() const
    {
        return name;
    }
    const std::optional<int>& GetParentId() const
    {
        return parentId;
    }
    bool AreChannelsLoaded() const
    {
        return channelsLoaded;
    }
    bool AreGroupsLoaded() const
    {
        return groupsLoaded;
    }

    template <typename P>
    void IterateGroups(P pred)
    {
        for (const auto& g : groups)
        {
            pred(g);
        }
    }
    template <typename P>
    void IterateChannels(P pred)
    {
        for (const auto& g : channels)
        {
            pred(g);
        }
    }

protected:
    int id;
    std::string name;
    std::optional<int> parentId;
    std::vector<ChannelsGroupPtr> groups;
    std::vector<ChannelPtr> channels;
    bool channelsLoaded = false;
    bool groupsLoaded = false;
};
