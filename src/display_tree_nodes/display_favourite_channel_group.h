#pragma once

#include "display_channel_group.h"
#include "display_root_channel_group.h"

struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
{
    DisplayFavouritesChannelsGroup(DisplayNodeKey,
                                   WorkersProvider* workersProvider,
                                   boost::asio::any_io_executor ui_executor,
                                   DisplayRootChannelsGroup* parent);
    static std::shared_ptr<DisplayFavouritesChannelsGroup>
    Create(WorkersProvider* workersProvider,
           boost::asio::any_io_executor ui_executor,
           DisplayRootChannelsGroup* parent)
    {
        return std::make_shared<DisplayFavouritesChannelsGroup>(
            DisplayNodeKey{}, workersProvider, ui_executor, parent);
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
