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
                          auto root = self->loadChannelsData();
                          boost::asio::post(cb_executor,
                                            [root, cb]() { cb(root); });
                      });
}
RootChannelsGroupPtr ChannelsRepository::loadChannelsData()
{
    auto root = std::make_shared<RootChannelsGroup>();
    auto groups = loadAllGroups();
    auto channels = loadAllChannels();

    for (const auto& channel : channels)
    {
        auto parentId = channel->GetParentId();
        if (parentId.has_value())
        {
            auto it = groups.find(parentId.value());
            if (it != groups.end())
            {
                it->second->AddChannel(channel);
            }
        }
        else
        {
            root->AddChannel(channel);
        }
        if (channel->IsFavourite())
        {
            root->AddFavouriteChannel(channel);
        }
    }

    for (const auto& entry : groups)
    {
        auto parentId = entry.second->GetParentId();
        if (parentId.has_value())
        {
            auto it = groups.find(parentId.value());
            if (it != groups.end())
            {
                it->second->AddChannelGroup(entry.second);
            }
        }
        else
        {
            root->AddChannelGroup(entry.second);
        }
    }

    return root;
}

std::map<int, ChannelsGroupPtr> ChannelsRepository::loadAllGroups()
{
    std::map<int, ChannelsGroupPtr> groups;
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = { (
        session.prepare << "SELECT GROUP_ID, NAME, PARENT_GROUP_ID FROM "
                           "CHANNEL_GROUPS ORDER BY GROUP_ID") };
    for (const auto& r : rows)
    {
        int id = r.get<int>(0);
        std::string name = r.get<std::string>(1);
        std::optional<int> parentId;
        if (r.get_indicator(2) == soci::i_ok)
        {
            parentId = r.get<int>(2);
        }
        auto group = std::make_shared<ChannelsGroup>(id, std::move(name),
                                                     std::move(parentId));
        groups.emplace(id, std::move(group));
    }

    return groups;
}
std::vector<ChannelPtr> ChannelsRepository::loadAllChannels()
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();

    soci::rowset<soci::row> rows = { (
        session.prepare
        << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, LOGO, EPG_CHANNEL_URI, "
           "EPG_CHANNEL_ID, XSTREAM_SERVER_ID, FAVOURITE, GROUP_ID FROM "
           "CHANNELS "
           "  ORDER BY GROUP_ID,CHANNEL_ID") };

    for (const auto& r : rows)
    {
        channels.push_back(loadChannel(r));
    }
    return channels;
}

std::vector<ChannelPtr> ChannelsRepository::loadFavourites()
{
    std::vector<ChannelPtr> channels;
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = {
        session.prepare
        << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, LOGO, EPG_CHANNEL_URI, "
           "EPG_CHANNEL_ID, XSTREAM_SERVER_ID, FAVOURITE, GROUP_ID FROM "
           "CHANNELS "
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
        auto group = std::make_shared<ChannelsGroup>(
            r.get<int>(0), r.get<std::string>(1), parentId);
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
               "EPG_CHANNEL_ID, XSTREAM_SERVER_ID, FAVOURITE, GROUP_ID FROM "
               "CHANNELS "
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
    int favourite = r.get<int>(8);
    std::optional<int> groupId;
    if (r.get_indicator(9) == soci::i_ok)
    {
        groupId = r.get<int>(9);
    }

    boost::algorithm::replace_all(name, "#",
                                  reinterpret_cast<const char*>(u8"\u2E30"));
    return std::make_shared<Channel>(
        id, std::move(name), std::move(uri), std::move(logo_uri),
        std::move(logo), std::move(epgChannelUri), std::move(epgChannelId),
        xstreamServerId, favourite == 1, std::move(groupId));
}