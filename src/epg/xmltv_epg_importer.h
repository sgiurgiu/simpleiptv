#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "../servers/server.h"
#include "epg_programme.h"
#include "xmltv_parser.h"

class WorkersProvider;

// Orchestrates a streaming XMLTV import for one server: downloads /xmltv.php,
// parses it incrementally on a dedicated strand (off the UI thread), and
// persists completed programmes to the DB in batches. The importer keeps itself
// alive (shared_from_this) for the duration of the download, so callers can
// fire-and-forget.
class XmlTvEpgImporter : public std::enable_shared_from_this<XmlTvEpgImporter>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    using DoneCallback = std::function<void(std::error_code)>;

    XmlTvEpgImporter(Key, WorkersProvider* workersProvider);
    static std::shared_ptr<XmlTvEpgImporter>
    Create(WorkersProvider* workersProvider);

    // done is invoked once on cb_executor when the import finishes, whether it
    // succeeded or failed.
    void Import(ServerPtr server,
                DoneCallback done,
                const boost::asio::any_io_executor& cb_executor);

private:
    void onChunk(std::string body, std::error_code ec);
    void flushBatch();
    void flushChannelBatch();
    void finish(std::error_code ec);

private:
    static constexpr std::size_t kBatchSize = 1000;

    WorkersProvider* workersProvider;
    boost::asio::strand<boost::asio::any_io_executor> strand;

    ServerPtr server;
    DoneCallback done;
    boost::asio::any_io_executor cbExecutor;
    std::unique_ptr<XmlTvParser> parser;
    std::vector<EpgProgramme> batch;
    std::vector<EpgChannelInfo> channelBatch;
    bool finished = false;
};
