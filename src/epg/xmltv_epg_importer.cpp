#include "xmltv_epg_importer.h"

#include <boost/asio/post.hpp>
#include <boost/url.hpp>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <system_error>
#include <utility>

#include "../workers_provider.h"

XmlTvEpgImporter::XmlTvEpgImporter(Key, WorkersProvider* workersProvider)
: workersProvider(workersProvider)
, strand(boost::asio::make_strand(workersProvider->GetNetworkExecutor()))
{
}

std::shared_ptr<XmlTvEpgImporter>
XmlTvEpgImporter::Create(WorkersProvider* workersProvider)
{
    return std::make_shared<XmlTvEpgImporter>(Key{}, workersProvider);
}

void XmlTvEpgImporter::Import(ServerPtr srv,
                              DoneCallback cb,
                              const boost::asio::any_io_executor& cb_executor)
{
    server = std::move(srv);
    done = std::move(cb);
    cbExecutor = cb_executor;
    batch.reserve(kBatchSize);
    channelBatch.reserve(kBatchSize);

    // Replace any existing EPG for this server before streaming in the new one.
    workersProvider->GetEpgRepository()->ClearServerProgrammes(server->GetId());
    workersProvider->GetEpgRepository()->ClearServerChannels(server->GetId());

    // The sinks run synchronously inside parser->Feed() on the strand, where the
    // importer is alive (held by the streaming callback below), so capturing a
    // raw `this` is safe and avoids a shared_ptr cycle through the parser.
    parser = std::make_unique<XmlTvParser>(
        [this](EpgProgramme&& p)
        {
            batch.push_back(std::move(p));
            if (batch.size() >= kBatchSize)
                flushBatch();
        },
        [this](EpgChannelInfo&& c)
        {
            channelBatch.push_back(std::move(c));
            if (channelBatch.size() >= kBatchSize)
                flushChannelBatch();
        });

    boost::url url;
    url.set_scheme(server->GetUrlScheme());
    url.set_host(server->GetHost());
    uint16_t port;
    auto [_, ec] = std::from_chars(
        server->GetPort().data(),
        server->GetPort().data() + server->GetPort().size(), port);
    if (ec == std::errc{})
    {
        url.set_port_number(port);
    }
    else
    {
        if ("https" == server->GetUrlScheme())
        {
            url.set_port_number(443);
        }
        else
        {
            url.set_port_number(80);
        }
    }
    url.set_path("/xmltv.php");
    auto params = url.params();
    params.set("username", server->GetUsername());
    params.set("password", server->GetPassword());

    workersProvider->GetNetworkResourceProvider()->GetResourceStreaming(
        url.buffer(), strand,
        [self = shared_from_this()](std::string body, std::error_code ec)
        { self->onChunk(std::move(body), ec); });
}

void XmlTvEpgImporter::onChunk(std::string body, std::error_code ec)
{
    if (finished)
        return;

    if (ec)
    {
        spdlog::error("XMLTV download error for server {}: {}", server->GetId(),
                      ec.message());
        finish(ec);
        return;
    }

    // Empty body with no error is the end-of-stream sentinel.
    const bool isEof = body.empty();

    if (!parser->Feed(body.data(), body.size(), isEof))
    {
        finish(std::make_error_code(std::errc::bad_message));
        return;
    }

    if (isEof)
    {
        flushChannelBatch();
        flushBatch();

        // Stamp freshness. Posted to the DB executor so it runs after the insert
        // batches (single DB thread => FIFO ordering) and never writes
        // concurrently with them.
        server->SetXmlTvUpdatedAt(std::chrono::system_clock::now());
        auto serversRepo = workersProvider->GetServersRepository();
        auto srv = server;
        boost::asio::post(workersProvider->GetDBExecutor(),
                          [serversRepo, srv]()
                          { serversRepo->UpdateServerXmlTvUpdatedAt(*srv); });

        spdlog::debug("Finished XMLTV import for server {}", server->GetId());
        finish(std::error_code{});
    }
}

void XmlTvEpgImporter::flushBatch()
{
    if (batch.empty())
        return;
    workersProvider->GetEpgRepository()->InsertProgrammes(server->GetId(),
                                                          std::move(batch));
    batch.clear();
    batch.reserve(kBatchSize);
}

void XmlTvEpgImporter::flushChannelBatch()
{
    if (channelBatch.empty())
        return;
    workersProvider->GetEpgRepository()->InsertChannels(server->GetId(),
                                                        std::move(channelBatch));
    channelBatch.clear();
    channelBatch.reserve(kBatchSize);
}

void XmlTvEpgImporter::finish(std::error_code ec)
{
    if (finished)
        return;
    finished = true;
    if (done)
    {
        boost::asio::post(cbExecutor,
                          [done = std::move(done), ec]() { done(ec); });
    }
}
