#include "epg_repository.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/post.hpp>
#include <cstdint>
#include <soci/soci.h>
#include <soci/use.h>
#include <spdlog/spdlog.h>
#include <utility>

#include "../channels/channel.h"
#include "../dbconnection_pool.h"
#include "../epg_listing.h"

namespace
{
// Build a Channel from a search row. The column order must match the SELECT in
// SearchProgrammes, and the mapping mirrors ChannelsRepository::loadChannel
// (including the '#' -> U+2E30 replacement that keeps names out of ImGui's "##"
// id syntax) so search results behave like channels loaded anywhere else.
ChannelPtr channelFromSearchRow(const soci::row& r)
{
    int id = r.get<int>(0, -1);
    auto name = r.get<std::string>(1, "");
    auto uri = r.get<std::string>(2, "");
    auto logoUri = r.get<std::string>(3, "");
    auto logo = r.get<std::string>(4, "");
    auto epgChannelUri = r.get<std::string>(5, "");
    auto epgChannelId = r.get<std::string>(6, "");
    int xstreamServerId = r.get<int>(7, -1);
    int favourite = r.get<int>(8, 0);
    std::optional<int> groupId;
    if (r.get_indicator(9) == soci::i_ok)
    {
        groupId = r.get<int>(9);
    }

    boost::algorithm::replace_all(name, "#",
                                  reinterpret_cast<const char*>(u8"⸰"));
    return std::make_shared<Channel>(
        id, std::move(name), std::move(uri), std::move(logoUri),
        std::move(logo), std::move(epgChannelUri), std::move(epgChannelId),
        xstreamServerId, favourite == 1, std::move(groupId));
}
} // namespace

EpgRepository::EpgRepository(Key, const boost::asio::any_io_executor& executor)
: executor{ executor }
{
}

std::shared_ptr<EpgRepository>
EpgRepository::Create(const boost::asio::any_io_executor& executor)
{
    return std::make_shared<EpgRepository>(Key{}, executor);
}

void EpgRepository::ClearServerProgrammes(int serverId)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), serverId]()
        {
            try
            {
                auto session = DatabaseConnections::GetConnection();
                session
                    << "DELETE FROM EPG_PROGRAMMES WHERE XSTREAM_SERVER_ID = :id",
                    soci::use(serverId, "id");
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot clear EPG for server {}: {}", serverId,
                              ex.what());
            }
        });
}

void EpgRepository::InsertProgrammes(int serverId,
                                     std::vector<EpgProgramme> programmes)
{
    if (programmes.empty())
        return;
    boost::asio::post(
        executor,
        [self = shared_from_this(), serverId,
         programmes = std::move(programmes)]() mutable
        {
            try
            {
                auto session = DatabaseConnections::GetConnection();
                soci::transaction tr(session);

                // Bind variables once and re-execute the prepared statement per
                // row; the surrounding transaction is what keeps a 100k-row
                // import fast (one fsync instead of one per row).
                std::string channelId;
                std::string title;
                std::string description;
                int64_t startTime = 0;
                int64_t stopTime = 0;
                soci::statement st =
                    (session.prepare
                         << "INSERT INTO EPG_PROGRAMMES(XSTREAM_SERVER_ID, "
                            "EPG_CHANNEL_ID, START_TIME, STOP_TIME, TITLE, "
                            "DESCRIPTION) VALUES(:sid, :cid, :start, :stop, "
                            ":title, :desc)",
                     soci::use(serverId, "sid"), soci::use(channelId, "cid"),
                     soci::use(startTime, "start"), soci::use(stopTime, "stop"),
                     soci::use(title, "title"), soci::use(description, "desc"));

                for (auto& p : programmes)
                {
                    channelId = std::move(p.channelId);
                    title = std::move(p.title);
                    description = std::move(p.description);
                    startTime = p.startTime;
                    stopTime = p.stopTime;
                    st.execute(true);
                }
                tr.commit();
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot insert EPG batch for server {}: {}",
                              serverId, ex.what());
            }
        });
}

void EpgRepository::ClearServerChannels(int serverId)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), serverId]()
        {
            try
            {
                auto session = DatabaseConnections::GetConnection();
                session
                    << "DELETE FROM EPG_CHANNELS WHERE XSTREAM_SERVER_ID = :id",
                    soci::use(serverId, "id");
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot clear EPG channels for server {}: {}",
                              serverId, ex.what());
            }
        });
}

void EpgRepository::InsertChannels(int serverId,
                                   std::vector<EpgChannelInfo> channels)
{
    if (channels.empty())
        return;
    boost::asio::post(
        executor,
        [self = shared_from_this(), serverId,
         channels = std::move(channels)]() mutable
        {
            try
            {
                auto session = DatabaseConnections::GetConnection();
                soci::transaction tr(session);

                std::string channelId;
                std::string displayName;
                soci::statement st =
                    (session.prepare
                         << "INSERT INTO EPG_CHANNELS(XSTREAM_SERVER_ID, "
                            "EPG_CHANNEL_ID, DISPLAY_NAME) VALUES(:sid, :cid, "
                            ":name)",
                     soci::use(serverId, "sid"), soci::use(channelId, "cid"),
                     soci::use(displayName, "name"));

                for (auto& c : channels)
                {
                    channelId = std::move(c.channelId);
                    displayName = std::move(c.displayName);
                    st.execute(true);
                }
                tr.commit();
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot insert EPG channel batch for server {}: {}",
                              serverId, ex.what());
            }
        });
}

void EpgRepository::GetProgrammes(int serverId,
                                  std::string epgChannelId,
                                  std::int64_t fromUnix,
                                  std::int64_t toUnix,
                                  LoadProgrammesCallback cb,
                                  const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), serverId,
         epgChannelId = std::move(epgChannelId), fromUnix, toUnix,
         cb = std::move(cb), cb_executor]() mutable
        {
            std::vector<EpgListing> listings;
            try
            {
                auto session = DatabaseConnections::GetConnection();

                int64_t startTime = 0;
                int64_t stopTime = 0;
                std::string title;
                std::string description;
                // A programme overlaps the window when it stops after the start
                // and starts before the end, so STOP_TIME/START_TIME are compared
                // against the opposite bounds.
                soci::statement st =
                    (session.prepare
                         << "SELECT START_TIME, STOP_TIME, TITLE, DESCRIPTION "
                            "FROM EPG_PROGRAMMES WHERE XSTREAM_SERVER_ID = :sid "
                            "AND EPG_CHANNEL_ID = :cid AND STOP_TIME > :from "
                            "AND START_TIME < :to ORDER BY START_TIME",
                     soci::use(serverId, "sid"),
                     soci::use(epgChannelId, "cid"), soci::use(fromUnix, "from"),
                     soci::use(toUnix, "to"), soci::into(startTime),
                     soci::into(stopTime), soci::into(title),
                     soci::into(description));
                st.execute();
                while (st.fetch())
                {
                    listings.push_back(EpgListing::FromProgramme(
                        startTime, stopTime, title, description));
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error(
                    "Cannot load EPG programmes for server {} channel {}: {}",
                    serverId, epgChannelId, ex.what());
            }

            boost::asio::post(cb_executor,
                              [listings = std::move(listings),
                               cb = std::move(cb)]() mutable
                              { cb(std::move(listings)); });
        });
}

void EpgRepository::SearchProgrammes(std::string query,
                                     int limit,
                                     int start,
                                     int end,
                                     SearchProgrammesCallback cb,
                                     const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), query = std::move(query), start, end, limit,
         cb = std::move(cb), cb_executor]() mutable
        {
            std::vector<EpgSearchResult> results;
            try
            {
                auto session = DatabaseConnections::GetConnection();

                // LEFT JOIN so a match surfaces even when its channel isn't in
                // CHANNELS (then it can be named via EPG_CHANNELS but not
                // played). A programme on several stored channels still yields
                // one playable row per channel (capped by the limit).
                std::string pattern = "%" + query + "%";
                soci::rowset<soci::row> rows = { (
                    session.prepare
                        << "SELECT C.CHANNEL_ID, C.NAME, C.URI, C.LOGO_URI, "
                           "C.LOGO, C.EPG_CHANNEL_URI, C.EPG_CHANNEL_ID, "
                           "C.XSTREAM_SERVER_ID, C.FAVOURITE, C.GROUP_ID, "
                           "P.START_TIME, P.STOP_TIME, P.TITLE, P.DESCRIPTION, "
                           "P.EPG_CHANNEL_ID, E.DISPLAY_NAME "
                           "FROM EPG_PROGRAMMES P "
                           "LEFT JOIN CHANNELS C ON "
                           "C.XSTREAM_SERVER_ID = P.XSTREAM_SERVER_ID AND "
                           "C.EPG_CHANNEL_ID = P.EPG_CHANNEL_ID "
                           "LEFT JOIN EPG_CHANNELS E ON "
                           "E.XSTREAM_SERVER_ID = P.XSTREAM_SERVER_ID AND "
                           "E.EPG_CHANNEL_ID = P.EPG_CHANNEL_ID "
                           "WHERE (P.TITLE LIKE :q OR P.DESCRIPTION LIKE :q) "
                           "AND P.START_TIME >= :start "
                           "AND P.STOP_TIME <= :end "
                           "ORDER BY P.START_TIME LIMIT :lim",
                    soci::use(pattern, "q"), soci::use(limit, "lim"),
                    soci::use(start, "start"), soci::use(end, "end")) };
                for (const auto& r : rows)
                {
                    auto startTime = r.get<int>(10, 0);
                    auto stopTime = r.get<int>(11, 0);
                    auto title = r.get<std::string>(12, "");
                    auto description = r.get<std::string>(13, "");

                    // A matched CHANNELS row means the result is playable; only
                    // then build a Channel (get<>(idx, default) would otherwise
                    // fabricate one from the NULL left-join columns).
                    ChannelPtr channel;
                    std::string channelName;
                    if (r.get_indicator(0) == soci::i_ok)
                    {
                        channel = channelFromSearchRow(r);
                        channelName = channel->GetName();
                    }
                    else if (r.get_indicator(15) == soci::i_ok &&
                             !r.get<std::string>(15, "").empty())
                    {
                        channelName = r.get<std::string>(15, "");
                    }
                    else
                    {
                        channelName = r.get<std::string>(14, "");
                    }

                    results.push_back(EpgSearchResult{
                        std::move(channel), std::move(channelName),
                        EpgListing::FromProgramme(startTime, stopTime,
                                                  std::move(title),
                                                  std::move(description)) });
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot search EPG programmes for '{}': {}",
                              query, ex.what());
            }

            boost::asio::post(cb_executor, [results = std::move(results),
                                            cb = std::move(cb)]() mutable
                              { cb(std::move(results)); });
        });
}
