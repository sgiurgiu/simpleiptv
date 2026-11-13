#include "channel.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

Channel::Channel(int id,
                 std::string name,
                 std::string uri,
                 std::string logoUri,
                 std::string logo,
                 std::string epgChannelUri,
                 std::string epgChannelId,
                 int xstreamServerId,
                 bool favourite,
                 std::optional<int> parentId)
: id{ id }
, name{ std::move(name) }
, uri{ std::move(uri) }
, logoUri{ std::move(logoUri) }
, logo{ std::move(logo) }
, epgChannelUri{ std::move(epgChannelUri) }
, epgChannelId{ std::move(epgChannelId) }
, xstreamServerId{ xstreamServerId }
, favourite{ favourite }
, parentId{ std::move(parentId) }
{
}

std::string Channel::ToJson() const
{
    nlohmann::json json;
    json["id"] = id;
    json["name"] = name;
    json["uri"] = uri;
    json["logoUri"] = logoUri;
    json["epgChannelUri"] = epgChannelUri;
    json["epgChannelId"] = epgChannelId;
    json["xstreamServerId"] = xstreamServerId;
    json["favourite"] = favourite;
    if (parentId.has_value())
    {
        json["parentId"] = parentId.value();
    }
    else
    {
        json["parentId"] = nullptr;
    }
    return json.dump();
}

std::shared_ptr<Channel> Channel::FromJson(const std::string& str)
{
    auto json = nlohmann::json::parse(str, nullptr, false);
    if (json.is_discarded() || !json.is_object())
    {
        return nullptr;
    }
    try
    {
        auto uri = json.value("uri", std::string{});
        if (uri.empty())
        {
            return nullptr;
        }
        std::optional<int> parentId;
        if (auto it = json.find("parentId");
            it != json.end() && it->is_number_integer())
        {
            parentId = it->get<int>();
        }
        return std::make_shared<Channel>(
            json.value("id", -1), json.value("name", std::string{}),
            std::move(uri), json.value("logoUri", std::string{}), "",
            json.value("epgChannelUri", std::string{}),
            json.value("epgChannelId", std::string{}),
            json.value("xstreamServerId", -1), json.value("favourite", false),
            std::move(parentId));
    }
    catch (const nlohmann::json::exception& ex)
    {
        // A field with the wrong type, e.g. a hand-edited snapshot.
        spdlog::warn("Ignoring malformed channel snapshot: {}", ex.what());
        return nullptr;
    }
}
