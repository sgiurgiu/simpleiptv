#include "channel.h"

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
