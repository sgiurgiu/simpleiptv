#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../epg_listing.h"
#include "epg_programme.h"

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

private:
    boost::asio::any_io_executor executor;
};
