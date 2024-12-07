#pragma once

#include "channels_group.h"

class RootChannelsGroup : public ChannelsGroup
{
public:
    RootChannelsGroup();
    void AddFavouriteChannel(ChannelPtr channel);
    void AddFavouriteChannels(std::vector<ChannelPtr> channels);
    void RemoveFavouriteChannel(int id);
    template <typename P>
    void IterateFavouriteChannels(P pred)
    {
        for (const auto& g : favourites)
        {
            pred(g);
        }
    }

private:
    std::vector<ChannelPtr> favourites;
};
using RootChannelsGroupPtr = std::shared_ptr<RootChannelsGroup>;