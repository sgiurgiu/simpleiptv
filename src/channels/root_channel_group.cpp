#include "root_channel_group.h"
#include <algorithm>

RootChannelsGroup::RootChannelsGroup() : ChannelsGroup(-1, "Root")
{
}
void RootChannelsGroup::AddFavouriteChannel(ChannelPtr channel)
{
    favourites.push_back(channel);
}
void RootChannelsGroup::RemoveFavouriteChannel(int id)
{
    favourites.erase(std::remove_if(favourites.begin(), favourites.end(),
                                    [id](ChannelPtr channel)
                                    { return channel->GetId() == id; }),
                     favourites.end());
}
void RootChannelsGroup::AddFavouriteChannels(std::vector<ChannelPtr> channels)
{
    favourites.insert(favourites.end(), channels.begin(), channels.end());
}