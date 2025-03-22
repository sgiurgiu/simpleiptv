#include "display_favourite_channel_group.h"

#include "../fonts/IconsFontAwesome4.h"

DisplayFavouritesChannelsGroup::DisplayFavouritesChannelsGroup(
    DisplayNodeKey key, DisplayRootChannelsGroup* parent)
: DisplayChannelsGroup{
    key, reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"), parent
}
{
    isOpen = true;
}
