#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <memory>
#include <vector>

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

private:
    boost::asio::any_io_executor executor;
};
