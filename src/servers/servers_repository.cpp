#include "servers_repository.h"

#include <boost/asio/post.hpp>
#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include "../dbconnection_pool.h"

ServersRepository::ServersRepository(Key,
                                     const boost::asio::any_io_executor& executor)
: executor{ executor }
{
}
std::shared_ptr<ServersRepository>
ServersRepository::Create(const boost::asio::any_io_executor& executor)
{
    return std::make_shared<ServersRepository>(Key{}, executor);
}

void ServersRepository::LoadServers(LoadServersCallback cb,
                                    const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb = std::move(cb), cb_executor]() mutable
        {
            std::vector<ServerPtr> servers;
            try
            {
                auto session = DatabaseConnections::GetConnection();

                soci::rowset<soci::row> rows = { (
                    session.prepare
                    << "SELECT SERVER_ID, HOST, PORT, "
                       "SERVER_URL_SCHEMA, USERNAME, "
                       "PASSWORD, TIMEZONE, IS_TRIAL, MAX_CONNECTIONS, "
                       "CREATED_AT, RTMP_PORT, HTTPS_PORT,"
                       "STATUS, EXPIRY_DATE FROM "
                       "XSTREAM_SERVERS ORDER BY HOST") };
                for (const auto& r : rows)
                {
                    int id = r.get<int>(0, -1);
                    auto host = r.get<std::string>(1, "");
                    auto port = r.get<std::string>(2, "");
                    auto urlScheme = r.get<std::string>(3, "");
                    auto username = r.get<std::string>(4, "");
                    auto password = r.get<std::string>(5, "");
                    auto timezone = r.get<std::string>(6, "");
                    bool trial = r.get<bool>(7, true);
                    int maxConnections = r.get<int>(8, 0);
                    int64_t createdAt = r.get<int64_t>(9, 0);
                    auto rtmpPort = r.get<std::string>(10, "");
                    auto httpsPort = r.get<std::string>(11, "");
                    auto status = r.get<std::string>(12, "");
                    int64_t expiryDate = r.get<int64_t>(13, 0);

                    soci::rowset<soci::row> formatRows = { (
                        session.prepare
                            << "SELECT FORMAT FROM "
                               "SERVER_OUTPUT_FORMATS WHERE XSTREAM_SERVER_ID=:ID",
                        soci::use(id)) };
                    std::vector<std::string> outputFormats;
                    for (const auto& fr : formatRows)
                    {
                        outputFormats.push_back(fr.get<std::string>(0, ""));
                    }

                    servers.push_back(std::make_shared<Server>(
                        id, host, port, urlScheme, username, password, timezone,
                        status, expiryDate, createdAt, trial, maxConnections,
                        rtmpPort, httpsPort, outputFormats));
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load servers: {}", ex.what());
            }

            boost::asio::post(cb_executor, [servers = std::move(servers),
                                            cb = std::move(cb)]() mutable
                              { cb(std::move(servers)); });
        });
}
void ServersRepository::AddServer(const Server& server,
                                  LoadServerCallback cb,
                                  const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb = std::move(cb), cb_executor, server]() mutable
        {
            ServerPtr serverPtr;
            try
            {
                auto session = DatabaseConnections::GetConnection();
                int id;
                std::string host = server.GetHost();
                std::string port = server.GetPort();
                std::string urlScheme = server.GetUrlScheme();
                std::string username = server.GetUsername();
                std::string password = server.GetPassword();
                std::string timezone = server.GetTimezone();
                std::string status = server.GetStatus();
                int expiryDate = server.GetExpiryDate();
                int createdAt = server.GetCreatedAt();
                int trial = server.IsTrial() ? 1 : 0;
                int maxConnections = server.GetMaxConnections();
                std::string rtmpPort = server.GetRTMPPort();
                std::string httpsPort = server.GetHTTPSPort();

                session << "INSERT INTO XSTREAM_SERVERS(HOST, PORT, "
                           "SERVER_URL_SCHEMA, USERNAME, PASSWORD, TIMEZONE, "
                           "STATUS, EXPIRY_DATE, IS_TRIAL, MAX_CONNECTIONS, "
                           "CREATED_AT, RTMP_PORT, HTTPS_PORT) VALUES(:host, "
                           ":port, :schema, :username, :password, :timezone, "
                           ":status, :exp_date, :trial, :max_con, :created, "
                           ":rtmp_port, :https_port) RETURNING SERVER_ID",
                    soci::use(host, "host"), soci::use(port, "port"),
                    soci::use(urlScheme, "schema"), soci::use(username, "username"),
                    soci::use(password, "password"), soci::use(timezone, "timezone"),
                    soci::use(status, "status"), soci::use(expiryDate, "exp_date"),
                    soci::use(maxConnections, "max_con"),
                    soci::use(createdAt, "created"),
                    soci::use(rtmpPort, "rtmp_port"),
                    soci::use(httpsPort, "https_port"), soci::use(trial, "trial"),
                    soci::into(id);
                for (const auto& f : server.GetOutputFormats())
                {
                    session
                        << "INSERT INTO SERVER_OUTPUT_FORMATS(XSTREAM_SERVER_ID, "
                           "FORMAT) VALUES(:id, :format)",
                        soci::use(id), soci::use(f);
                }
                serverPtr = std::make_shared<Server>(
                    id, server.GetHost(), server.GetPort(), server.GetUrlScheme(),
                    server.GetUsername(), server.GetPassword(),
                    server.GetTimezone(), server.GetStatus(),
                    server.GetExpiryDate(), server.GetCreatedAt(), server.IsTrial(),
                    server.GetMaxConnections(), server.GetRTMPPort(),
                    server.GetHTTPSPort(), server.GetOutputFormats());
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot add server '{}': {}", server.GetHost(),
                              ex.what());
            }
            boost::asio::post(cb_executor, [serverPtr = std::move(serverPtr),
                                            cb = std::move(cb)]() mutable
                              { cb(std::move(serverPtr)); });
        });
}
