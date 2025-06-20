#include "channels_group.h"

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
    std::erase_if(groups,
                  [id](ChannelsGroupPtr group) { return group->id == id; });
}
void ChannelsGroup::RemoveChannel(int id)
{
    std::erase_if(channels,
                  [id](ChannelPtr channel) { return channel->GetId() == id; });
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
