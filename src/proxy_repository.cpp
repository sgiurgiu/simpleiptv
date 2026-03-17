#include "proxy_repository.h"
#include <boost/asio/post.hpp>

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include "dbconnection_pool.h"

ProxyRepository::ProxyRepository(Key, const boost::asio::any_io_executor& executor)
: executor{ executor }
{
}
std::shared_ptr<ProxyRepository>
ProxyRepository::Create(const boost::asio::any_io_executor& executor)
{
    return std::make_shared<ProxyRepository>(Key{}, executor);
}

void ProxyRepository::LoadConfiguredProxy(
    LoadProxyCallback cb, const boost::asio::any_io_executor& cb_executor)
{
    boost::asio::post(
        executor,
        [self = shared_from_this(), cb, cb_executor]()
        {
            HttpProxy proxy;
            try
            {
                auto session = DatabaseConnections::GetConnection();
                int use = 0;
                soci::indicator hostInd = soci::i_null;
                soci::indicator portInd = soci::i_null;
                soci::indicator useInd = soci::i_null;
                session << "SELECT HOST, PORT, USE FROM HTTP_PROXY LIMIT 1",
                    soci::into(proxy.host, hostInd),
                    soci::into(proxy.port, portInd), soci::into(use, useInd);
                if (hostInd == soci::i_ok && portInd == soci::i_ok)
                {
                    proxy.use = (useInd == soci::i_ok && use == 1);
                }
            }
            catch (const soci::soci_error& ex)
            {
                spdlog::error("Cannot load configured proxy: {}", ex.what());
            }

            boost::asio::post(cb_executor,
                              [cb, proxy]() { cb(std::move(proxy)); });
        });
}
void ProxyRepository::SaveConfiguredProxy(HttpProxy proxy)
{
    boost::asio::post(executor,
                      [self = shared_from_this(), proxy]()
                      {
                          try
                          {
                              auto session = DatabaseConnections::GetConnection();
                              int use = proxy.use ? 1 : 0;
                              session << "DELETE FROM HTTP_PROXY";
                              session << "INSERT INTO HTTP_PROXY (HOST, PORT, USE) "
                                         "VALUES (:host, :port, :use)",
                                  soci::use(proxy.host), soci::use(proxy.port),
                                  soci::use(use);
                              self->proxySettingsSignal(std::move(proxy));
                          }
                          catch (const soci::soci_error& ex)
                          {
                              spdlog::error("Cannot save configured proxy: {}",
                                            ex.what());
                          }
                      });
}