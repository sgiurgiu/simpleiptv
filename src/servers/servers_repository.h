#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "server.h"

class ServersRepository : public std::enable_shared_from_this<ServersRepository>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ServersRepository(Key, const boost::asio::any_io_executor& executor);
    static std::shared_ptr<ServersRepository>
    Create(const boost::asio::any_io_executor& executor);

    using LoadServersCallback = std::function<void(std::vector<ServerPtr>)>;
    using LoadServerCallback = std::function<void(ServerPtr)>;
    // the callback will be called on the cb_executor provided
    void LoadServers(LoadServersCallback cb,
                     const boost::asio::any_io_executor& cb_executor);
    void AddServer(const Server& server,
                   LoadServerCallback cb,
                   const boost::asio::any_io_executor& cb_executor);
    void UpdateServerXmlTvUpdatedAt(const Server& server);

private:
private:
    boost::asio::any_io_executor executor;
};
