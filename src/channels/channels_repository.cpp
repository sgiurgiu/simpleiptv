#include "channels_repository.h"

#include "../dbconnection_pool.h"
#include "channels_group.h"

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/post.hpp>
#include <iterator>
#include <memory>
#include <optional>
#include <soci/soci.h>
#include <soci/use.h>
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
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb = std::move(cb), cb_executor]() mutable
        {
            RootChannelsGroupPtr root;
            try
            {
                root = self->loadChannelsData();
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load channels and groups: {}", ex.what());
                root = std::make_shared<RootChannelsGroup>();
            }
            boost::asio::post(cb_executor, [root = std::move(root),
                                            cb = std::move(cb)]() mutable
                              { cb(std::move(root)); });
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
        groups.emplace(id, loadGroup(r));
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
        session.prepare << "SELECT GROUP_ID, NAME, PARENT_GROUP_ID FROM "
                           "CHANNEL_GROUPS WHERE IIF(:id IS NULL, "
                           "PARENT_GROUP_ID IS NULL, PARENT_GROUP_ID=:id) "
                           "ORDER BY GROUP_ID",
        soci::use(id, ind, "id")) };
    for (const auto& r : rows)
    {
        groups.push_back(loadGroup(r));
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

void ChannelsRepository::GetFavouritesPage(
    int page,
    int channelsPerPage,
    LoadChannelsCallback cb,
    const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), page, channelsPerPage, cb = std::move(cb),
         cb_executor]() mutable
        {
            std::vector<ChannelPtr> channels;
            int total = 0;
            try
            {
                auto session = DatabaseConnections::GetConnection();
                session << "SELECT COUNT(*) FROM CHANNELS WHERE FAVOURITE=TRUE",
                    soci::into(total);

                soci::rowset<soci::row> rows = { (
                    session.prepare
                        << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, "
                           "LOGO, EPG_CHANNEL_URI, "
                           "EPG_CHANNEL_ID, XSTREAM_SERVER_ID, "
                           "FAVOURITE, GROUP_ID FROM "
                           "CHANNELS "
                           "WHERE FAVOURITE=TRUE ORDER BY NAME LIMIT :size "
                           "OFFSET :offset",
                    soci::use(channelsPerPage),
                    soci::use(page * channelsPerPage)) };
                for (const auto& r : rows)
                {
                    channels.push_back(self->loadChannel(r));
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load favourites page {}: {}", page,
                              ex.what());
            }

            boost::asio::post(cb_executor, [channels = std::move(channels),
                                            cb = std::move(cb), total]() mutable
                              { cb(std::move(channels), total); });
        });
}
void ChannelsRepository::GetChannelsPage(
    ChannelsGroupPtr group,
    int page,
    int channelsPerPage,
    LoadChannelsCallback cb,
    const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), group = std::move(group), page,
         channelsPerPage, cb = std::move(cb), cb_executor]() mutable
        {
            int id = group ? group->GetId() : 0;
            auto ind = group ? soci::i_ok : soci::i_null;
            int total = 0;
            std::vector<ChannelPtr> channels;
            try
            {
                auto session = DatabaseConnections::GetConnection();

                session << "SELECT COUNT(*) FROM CHANNELS WHERE IIF(:id IS NULL, "
                           "GROUP_ID IS NULL, GROUP_ID=:id)",
                    soci::use(id, ind, "id"), soci::into(total);

                soci::rowset<soci::row> rows = { (
                    session.prepare << "SELECT CHANNEL_ID, NAME, URI, LOGO_URI, "
                                       "LOGO, EPG_CHANNEL_URI, "
                                       "EPG_CHANNEL_ID, XSTREAM_SERVER_ID, "
                                       "FAVOURITE, GROUP_ID FROM "
                                       "CHANNELS "
                                       "WHERE IIF(:id IS NULL, "
                                       "GROUP_ID IS NULL, GROUP_ID=:id)   ORDER BY "
                                       "NAME LIMIT :size "
                                       "OFFSET :offset",
                    soci::use(id, ind, "id"), soci::use(channelsPerPage, "size"),
                    soci::use(page * channelsPerPage, "offset")) };
                for (const auto& r : rows)
                {
                    channels.push_back(self->loadChannel(r));
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load channels page {}: {}", page,
                              ex.what());
            }

            boost::asio::post(cb_executor, [channels = std::move(channels),
                                            cb = std::move(cb), total]() mutable
                              { cb(std::move(channels), total); });
        });
}
void ChannelsRepository::GetGroups(LoadGroupsCallback cb,
                                   const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb = std::move(cb), cb_executor]() mutable
        {
            std::vector<ChannelsGroupPtr> groups;
            try
            {
                auto groupsMap = self->loadAllGroups();
                std::transform(groupsMap.begin(), groupsMap.end(),
                               std::back_inserter(groups),
                               [](auto& kv) { return kv.second; });
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load groups: {}", ex.what());
            }
            boost::asio::post(cb_executor, [groups = std::move(groups),
                                            cb = std::move(cb)]() mutable
                              { cb(std::move(groups)); });
        });
}

void ChannelsRepository::UpdateChannelLogo(int id, std::string logo)
{
    boost::asio::post(executor, [self = shared_from_this(), id,
                                 logo = std::move(logo)]() mutable
                      { self->UpdateChannelLogoSync(id, std::move(logo)); });
}
void ChannelsRepository::UpdateChannelLogoSync(int id, std::string logo)
{
    auto session = DatabaseConnections::GetConnection();

    session << "UPDATE CHANNELS SET LOGO=:logo WHERE CHANNEL_ID=:id",
        soci::use(logo, "logo"), soci::use(id, "id");
}
void ChannelsRepository::UpdateChannelFavourite(int id, bool favourite)
{
    boost::asio::post(executor, [self = shared_from_this(), id, favourite]() mutable
                      { self->UpdateChannelFavouriteSync(id, favourite); });
}
void ChannelsRepository::UpdateChannelFavouriteSync(int id, bool favourite)
{
    auto session = DatabaseConnections::GetConnection();

    session << "UPDATE CHANNELS SET FAVOURITE=:favourite WHERE CHANNEL_ID=:id",
        soci::use(favourite ? 1 : 0, "favourite"), soci::use(id, "id");
}
ChannelsGroupPtr ChannelsRepository::findGroup(const std::string& name)
{
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = { (
        session.prepare
            << "SELECT GROUP_ID, NAME, PARENT_GROUP_ID FROM "
               "CHANNEL_GROUPS WHERE NAME = :name ORDER BY GROUP_ID",
        soci::use(name, "name")) };
    for (const auto& r : rows)
    {
        return loadGroup(r);
    }
    return ChannelsGroupPtr{};
}

ChannelsGroupPtr ChannelsRepository::loadGroup(const soci::row& r)
{
    int id = r.get<int>(0);
    std::string name = r.get<std::string>(1);
    std::optional<int> parentId;
    if (r.get_indicator(2) == soci::i_ok)
    {
        parentId = r.get<int>(2);
    }
    return std::make_shared<ChannelsGroup>(id, std::move(name),
                                           std::move(parentId));
}

void ChannelsRepository::SaveGroup(ChannelsGroupPtr group,
                                   SaveGroupCallback cb,
                                   const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), group = std::move(group),
         cb = std::move(cb), cb_executor]() mutable
        {
            std::optional<int> id;
            try
            {
                auto session = DatabaseConnections::GetConnection();
                soci::rowset<soci::row> rows = { (
                    session.prepare
                        << "INSERT INTO CHANNEL_GROUPS(NAME) VALUES(:name) "
                           "RETURNING GROUP_ID ",
                    soci::use(group->GetName(), "name")) };
                for (const auto& r : rows)
                {
                    id = r.get<int>(0);
                    break;
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot save group '{}': {}", group->GetName(),
                              ex.what());
            }
            if (id)
            {
                ChannelsGroupPtr g = std::make_shared<ChannelsGroup>(
                    id.value(), group->GetName(), std::nullopt);
                group->IterateChannels([g, self](auto channel)
                                       { self->upsertChannel(channel, g); });
                boost::asio::post(cb_executor, [g, cb = std::move(cb)]() mutable
                                  { cb(std::move(g)); });
            }
            else
            {
                boost::asio::post(cb_executor, [cb = std::move(cb)]() mutable
                                  { cb(ChannelsGroupPtr{}); });
            }
        });
}

// update or insert group and its children
void ChannelsRepository::UpsertGroup(ChannelsGroupPtr group,
                                     SaveGroupCallback cb,
                                     const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(executor,
                      [self = shared_from_this(), group = std::move(group),
                       cb = std::move(cb), cb_executor]() mutable
                      {
                          try
                          {
                              auto foundGroup = self->findGroup(group->GetName());
                              if (foundGroup)
                              {
                                  group->IterateChannels(
                                      [self, foundGroup](auto channel)
                                      { self->upsertChannel(channel, foundGroup); });
                                  boost::asio::post(
                                      cb_executor,
                                      [group, cb = std::move(cb)]() mutable
                                      { cb(std::move(group)); });
                              }
                              else
                              {
                                  self->SaveGroup(group, std::move(cb), cb_executor);
                              }
                          }
                          catch (const soci::soci_error& ex)
                          {
                              spdlog::error("Cannot upsert group '{}': {}",
                                            group->GetName(), ex.what());
                              boost::asio::post(
                                  cb_executor,
                                  [cb = std::move(cb)]() mutable
                                  { cb(ChannelsGroupPtr{}); });
                          }
                      });
}

ChannelPtr ChannelsRepository::upsertChannel(ChannelPtr channel,
                                             ChannelsGroupPtr parent)
{
    auto session = DatabaseConnections::GetConnection();
    soci::rowset<soci::row> rows = { (
        session.prepare << "SELECT CHANNEL_ID FROM CHANNELS WHERE URI = :uri",
        soci::use(channel->GetUri(), "uri")) };
    std::optional<int> id;
    for (const auto& r : rows)
    {
        id = r.get<int>(0);
        break;
    }

    if (id)
    {
        session << "UPDATE CHANNELS SET NAME=:name, LOGO_URI = :logo_uri, "
                   "LOGO = NULL, EPG_CHANNEL_URI = :epg_uri, "
                   "EPG_CHANNEL_ID = :epg_id, XSTREAM_SERVER_ID = "
                   ":xstream_server_id, "
                   " GROUP_ID = :group_id "
                   "WHERE CHANNEL_ID=:id ",
            soci::use(channel->GetName(), "name"),
            soci::use(channel->GetLogoUri(), "logo_uri"),
            soci::use(channel->GetEPGChannelUri(), "epg_uri"),
            soci::use(channel->GetEPGChannelId(), "epg_id"),
            soci::use(channel->GetXStreamServerId(), "xstream_server_id"),
            soci::use(parent->GetId(), "group_id"), soci::use(id.value(), "id");
    }
    else
    {
        rows = { (session.prepare
                      << "INSERT INTO CHANNELS (NAME, URI, LOGO_URI, "
                         "EPG_CHANNEL_URI, EPG_CHANNEL_ID, XSTREAM_SERVER_ID, "
                         "GROUP_ID)"
                         " VALUES(:name, :uri, :logo_uri, :epg_uri, :epg_id, "
                         ":xstream_server_id, :group_id) RETURNING CHANNEL_ID",
                  soci::use(channel->GetName(), "name"),
                  soci::use(channel->GetUri(), "uri"),
                  soci::use(channel->GetLogoUri(), "logo_uri"),
                  soci::use(channel->GetEPGChannelUri(), "epg_uri"),
                  soci::use(channel->GetEPGChannelId(), "epg_id"),
                  soci::use(channel->GetXStreamServerId(), "xstream_server_id"),
                  soci::use(parent->GetId(), "group_id")) };
        for (const auto& r : rows)
        {
            id = r.get<int>(0);
            break;
        }
        // TODO: actually set the channel id, return a new object, etc.
    }
    return channel;
}