#include "channels_repository.h"

#include "../dbconnection_pool.h"

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/post.hpp>
#include <soci/soci.h>
#include <spdlog/spdlog.h>

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

void ChannelsRepository::LoadChannelsAndGroups(
    LoadRootCallback cb, const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(executor,
                      [self = shared_from_this(), cb, cb_executor]()
                      {
                          auto root = self->loadAllChannels(cb_executor);
                          boost::asio::post(cb_executor,
                                            [root, cb]() { cb(root); });
                      });
}
RootChannelsGroupPtr ChannelsRepository::loadAllChannels(
    const boost::asio::any_io_executor& cb_executor)
{
    auto root = std::make_shared<RootChannelsGroup>();
    auto favourites = loadFavourites();
    root->AddFavouriteChannels(favourites);
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb_executor, root]()
        {
            auto rootGroups = self->loadGroups({});
            root->AddChannelGroups(rootGroups);
            root->AddChannels(self->loadChannels({}));
            for (auto& group : rootGroups)
            {
                boost::asio::post(
                    self->executor,
                    [self = self->shared_from_this(), cb_executor, group]()
                    { self->loadGroup(group, cb_executor); });
            }
        });

    return root;
}
std::vector<ChannelPtr> ChannelsRepository::loadFavourites()
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = {
        session.prepare
        << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, LOGO, EPG_CHANNEL_URI, "
           "EPG_CHANNEL_ID, XSTREAM_SERVER_ID FROM CHANNELS "
           "WHERE FAVOURITE=TRUE ORDER BY NAME"
    };
    for (const auto& r : rows)
    {
        channels.push_back(loadChannel(r));
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
                           "PARENT_GROUP_ID IS NULL, PARENT_GROUP_ID=:id) "
                           "ORDER BY GROUP_ID",
        soci::use(id, ind, "id")) };
    for (const auto& r : rows)
    {
        auto group = std::make_shared<ChannelsGroup>(r.get<int>(0),
                                                     r.get<std::string>(1));
        groups.push_back(group);
    }

    return groups;
}
void ChannelsRepository::loadGroup(ChannelsGroupPtr group,
                                   const boost::asio::any_io_executor& cb_executor)
{
    auto groups = loadGroups(group->GetId());
    boost::asio::post(cb_executor,
                      [group, groups]() { group->AddChannelGroups(groups); });

    for (auto g : groups)
    {
        boost::asio::post(executor, [self = shared_from_this(), cb_executor, g]()
                          { self->loadGroup(g, cb_executor); });
    }
    auto channels = loadChannels(group);
    boost::asio::post(cb_executor,
                      [group, channels]() { group->AddChannels(channels); });
}

std::vector<ChannelPtr> ChannelsRepository::loadChannels(ChannelsGroupPtr group)
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();

    int id = group ? group->GetId() : 0;
    auto ind = group ? soci::i_ok : soci::i_null;

    soci::rowset<soci::row> rows = { (
        session.prepare
            << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, LOGO, EPG_CHANNEL_URI, "
               "EPG_CHANNEL_ID, XSTREAM_SERVER_ID FROM CHANNELS "
               " WHERE IIF(:id IS NULL, "
               "GROUP_ID IS NULL, GROUP_ID=:id)  ORDER BY CHANNEL_ID",
        soci::use(id, ind, "id")) };

    for (const auto& r : rows)
    {
        channels.push_back(loadChannel(r));
    }
    return channels;
}

ChannelPtr ChannelsRepository::loadChannel(const soci::row& r)
{
    int id = r.get<int>(0, -1);
    auto name = r.get<std::string>(1, "");
    auto uri = r.get<std::string>(2, "");
    auto logo_uri = r.get<std::string>(3, "");
    auto logo = r.get<std::string>(4, "");
    auto epgChannelUri = r.get<std::string>(5, "");
    auto epgChannelId = r.get<std::string>(6, "");
    int xstreamServerId = r.get<int>(7, -1);

    boost::algorithm::replace_all(name, "#",
                                  reinterpret_cast<const char*>(u8"\u2E30"));
    return std::make_shared<Channel>(id, std::move(name), std::move(uri),
                                     std::move(logo_uri), std::move(logo),
                                     std::move(epgChannelUri),
                                     std::move(epgChannelId), xstreamServerId);
}