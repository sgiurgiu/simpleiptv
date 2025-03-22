#pragma once

#include "display_channel_group.h"
#include "display_root_channel_group.h"

struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
{
    DisplayFavouritesChannelsGroup(DisplayNodeKey,
                                   DisplayRootChannelsGroup* parent);
    static std::shared_ptr<DisplayFavouritesChannelsGroup>
    Create(DisplayRootChannelsGroup* parent)
    {
        return std::make_shared<DisplayFavouritesChannelsGroup>(DisplayNodeKey{},
                                                                parent);
    }
    virtual int getUnderlyingID() const override
    {
        return -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::FAVOURITES;
    }
};
