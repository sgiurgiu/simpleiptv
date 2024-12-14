#include "channels_group.h"

#include <algorithm>

ChannelsGroup::ChannelsGroup(int id, std::string name, std::optional<int> parentId)
: id{ id }, name{ std::move(name) }, parentId{ std::move(parentId) }
{
}
void ChannelsGroup::AddChannelGroup(ChannelsGroupPtr group)
{
    groups.push_back(std::move(group));
    groupsLoaded = true;
}
void ChannelsGroup::AddChannel(ChannelPtr channel)
{
    channels.push_back(std::move(channel));
    channelsLoaded = true;
}
void ChannelsGroup::RemoveChannelGroup(int id)
{
    groups.erase(std::remove_if(groups.begin(), groups.end(),
                                [id](ChannelsGroupPtr group)
                                { return group->id == id; }),
                 groups.end());
}
void ChannelsGroup::RemoveChannel(int id)
{
    channels.erase(std::remove_if(channels.begin(), channels.end(),
                                  [id](ChannelPtr channel)
                                  { return channel->GetId() == id; }),
                   channels.end());
}
void ChannelsGroup::AddChannels(std::vector<ChannelPtr> channels)
{
    this->channels.insert(this->channels.end(), channels.begin(), channels.end());
    channelsLoaded = true;
}
void ChannelsGroup::AddChannelGroups(std::vector<ChannelsGroupPtr> groups)
{
    this->groups.insert(this->groups.end(), groups.begin(), groups.end());
    groupsLoaded = true;
}