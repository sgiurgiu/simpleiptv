#include "proxy_repository.h"
#include <boost/asio/post.hpp>

#include <soci/soci.h>

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
            auto session = DatabaseConnections::GetConnection();
            HttpProxy proxy;
            int use;
            session << "SELECT HOST, PORT, USE FROM HTTP_PROXY LIMIT 1",
                soci::into(proxy.host), soci::into(proxy.port), soci::into(use);
            proxy.use = (use == 1);

            boost::asio::post(cb_executor, [cb, proxy]() { cb(proxy); });
        });
}
void ProxyRepository::SaveConfiguredProxy(HttpProxy proxy)
{
    boost::asio::post(executor,
                      [self = shared_from_this(), proxy]()
                      {
                          auto session = DatabaseConnections::GetConnection();
                          int use = proxy.use ? 1 : 0;
                          session << "DELETE FROM HTTP_PROXY";
                          session << "INSERT INTO HTTP_PROXY (HOST, PORT, USE) "
                                     "VALUES (:host, :port, :use)",
                              soci::use(proxy.host), soci::use(proxy.port),
                              soci::use(use);
                      });
}