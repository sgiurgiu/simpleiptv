#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../channels/channel.h"
#include "../epg_listing.h"
#include "epg_programme.h"

// A single text-search hit: the matched programme together with the channel it
// belongs to (resolved by joining EPG_PROGRAMMES to CHANNELS), so the UI can
// both label the result and activate the channel when clicked.
struct EpgSearchResult
{
    // Null when the programme's channel isn't in CHANNELS, i.e. it cannot be
    // played directly from search.
    ChannelPtr channel;
    // Always set: the stored channel name, else the XMLTV display-name, else the
    // raw EPG channel id as a last resort.
    std::string channelName;
    EpgListing listing;
};

class EpgRepository : public std::enable_shared_from_this<EpgRepository>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    EpgRepository(Key, const boost::asio::any_io_executor& executor);
    static std::shared_ptr<EpgRepository>
    Create(const boost::asio::any_io_executor& executor);

    // Deletes all stored programmes for a server. Posted to the DB executor.
    void ClearServerProgrammes(int serverId);

    // Inserts a batch of programmes for a server in a single transaction.
    // Posted to the DB executor.
    void InsertProgrammes(int serverId, std::vector<EpgProgramme> programmes);

    // Deletes all stored channel names for a server. Posted to the DB executor.
    void ClearServerChannels(int serverId);

    // Inserts a batch of channel names for a server in a single transaction.
    // Posted to the DB executor.
    void InsertChannels(int serverId, std::vector<EpgChannelInfo> channels);

    using LoadProgrammesCallback = std::function<void(std::vector<EpgListing>)>;
    // Loads the stored programmes for a server's EPG channel that overlap the
    // [fromUnix, toUnix) window (unix seconds, UTC), ordered chronologically.
    // The query runs on the DB executor; the callback is posted to cb_executor.
    void GetProgrammes(int serverId,
                       std::string epgChannelId,
                       std::int64_t fromUnix,
                       std::int64_t toUnix,
                       LoadProgrammesCallback cb,
                       const boost::asio::any_io_executor& cb_executor);

    using SearchProgrammesCallback =
        std::function<void(std::vector<EpgSearchResult>)>;
    // Searches every stored programme whose title or description contains
    // `query` (SQL LIKE %query%), across all servers and channels, ordered by
    // start time and capped at `limit` rows. The query runs on the DB executor;
    // the callback is posted to cb_executor.
    void SearchProgrammes(std::string query,
                          int limit,
                          int start,
                          int end,
                          SearchProgrammesCallback cb,
                          const boost::asio::any_io_executor& cb_executor);

private:
    boost::asio::any_io_executor executor;
};
