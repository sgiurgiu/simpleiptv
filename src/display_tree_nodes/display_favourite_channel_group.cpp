#include "display_favourite_channel_group.h"

#include "../fonts/IconsFontAwesome4.h"

DisplayFavouritesChannelsGroup::DisplayFavouritesChannelsGroup(
    DisplayNodeKey key,
    WorkersProvider* workersProvider,
    boost::asio::any_io_executor ui_executor,
    DisplayRootChannelsGroup* parent)
: DisplayChannelsGroup{
    key, reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"),
    workersProvider, ui_executor, parent
}
{
    isOpen = true;
}
