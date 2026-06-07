#include "server.h"

Server::Server(int id,
               std::string host,
               std::string port,
               std::string urlScheme,
               std::string username,
               std::string password,
               std::string timezone,
               std::string status,
               int expiryDate,
               int createdAt,
               bool trial,
               int maxConnections,
               std::string rtmpPort,
               std::string httpsPort,
               std::vector<std::string> outputFormats,
               std::optional<std::chrono::system_clock::time_point> xmltvUpdatedAt)
: id{ id }
, host{ std::move(host) }
, port{ std::move(port) }
, urlScheme{ std::move(urlScheme) }
, username{ std::move(username) }
, password{ std::move(password) }
, timezone{ std::move(timezone) }
, status{ std::move(status) }
, expiryDate{ expiryDate }
, createdAt{ createdAt }
, trial{ trial }
, maxConnections{ maxConnections }
, rtmpPort{ std::move(rtmpPort) }
, httpsPort{ std::move(httpsPort) }
, xmltvUpdatedAt{ xmltvUpdatedAt }
, outputFormats{ std::move(outputFormats) }
{
}
