#include "channels_repository.h"

#include "../dbconnection_pool.h"

#include <boost/asio/post.hpp>
#include <soci/soci.h>

ChannelsRepository::ChannelsRepository(Key,
                                       const boost::asio::any_io_executor& executor)
: executor{ executor }
{
}
std::shared_ptr<ChannelsRepository>
ChannelsRepository::Create(const boost::asio::any_io_executor& executor)
{
    return std::make_shared<ChannelsRepository>(Key{}, executor);
}

void ChannelsRepository::LoadChannelsAndGroups(LoadRootCallback cb)
{
    boost::asio::post(executor, [self = shared_from_this(), cb]()
                      { cb(self->loadAllChannels()); });
}
RootChannelsGroupPtr ChannelsRepository::loadAllChannels()
{
    auto root = std::make_shared<RootChannelsGroup>();
    root->AddFavouriteChannels(loadFavourites());
    auto rootGroups = loadGroups({});
    root->AddChannelGroups(rootGroups);
    for (auto group : rootGroups)
    {
        loadGroup(group);
    }
    root->AddChannels(loadChannels({}));
    return root;
}
std::vector<ChannelPtr> ChannelsRepository::loadFavourites()
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = {
        session.prepare
        << "SELECT CHANNEL_ID, NAME FROM CHANNELS WHERE FAVOURITE=TRUE"
    };
    for (const auto& r : rows)
    {
        auto channel =
            std::make_shared<Channel>(r.get<int>(0), r.get<std::string>(1));
        channels.push_back(channel);
    }
    return channels;
}
std::vector<ChannelsGroupPtr>
ChannelsRepository::loadGroups(std::optional<int> parentId)
{
    std::vector<ChannelsGroupPtr> groups;
    auto session = DatabaseConnections::GetConnection();

    int id = parentId.value_or(0);
    auto ind = parentId ? soci::i_ok : soci::i_null;

    soci::rowset<soci::row> rows = { (
        session.prepare << "SELECT GROUP_ID, NAME FROM "
                           "CHANNEL_GROUPS WHERE IIF(:id IS NULL, "
                           "PARENT_GROUP_ID IS NULL, PARENT_GROUP_ID=:id)",
        soci::use(id, ind, "id")) };
    for (const auto& r : rows)
    {
        auto group = std::make_shared<ChannelsGroup>(r.get<int>(0),
                                                     r.get<std::string>(1));
        groups.push_back(group);
    }

    return groups;
}
void ChannelsRepository::loadGroup(ChannelsGroupPtr group)
{
    auto groups = loadGroups(group->GetId());
    group->AddChannelGroups(groups);
    for (auto g : groups)
    {
        loadGroup(g);
    }
    group->AddChannels(loadChannels(group));
}

std::vector<ChannelPtr> ChannelsRepository::loadChannels(ChannelsGroupPtr group)
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();

    int id = group ? group->GetId() : 0;
    auto ind = group ? soci::i_ok : soci::i_null;

    soci::rowset<soci::row> rows = { (session.prepare
                                          << "SELECT CHANNEL_ID, NAME FROM "
                                             "CHANNELS WHERE IIF(:id IS NULL, "
                                             "GROUP_ID IS NULL, GROUP_ID=:id)",
                                      soci::use(id, ind, "id")) };

    for (const auto& r : rows)
    {
        auto channel =
            std::make_shared<Channel>(r.get<int>(0), r.get<std::string>(1));
        channels.push_back(channel);
    }
    return channels;
}