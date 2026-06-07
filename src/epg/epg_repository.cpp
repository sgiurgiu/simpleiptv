#include "epg_repository.h"

#include <boost/asio/post.hpp>
#include <soci/soci.h>
#include <soci/use.h>
#include <spdlog/spdlog.h>
#include <utility>

#include "../dbconnection_pool.h"

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
                long long startTime = 0;
                long long stopTime = 0;
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
