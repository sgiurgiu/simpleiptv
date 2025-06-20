#pragma once

#include "display_channel_group.h"
#include "display_node.h"

#include <nlohmann/json.hpp>

#include "../servers/server.h"

struct DisplayServer : public DisplayChannelsGroup
{
    DisplayServer(DisplayNodeKey key,
                  WorkersProvider* workersProvider,
                  const boost::asio::any_io_executor& ui_executor,
                  ServerPtr server);
    static std::shared_ptr<DisplayServer>
    Create(WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor,
           ServerPtr server)
    {
        return std::make_shared<DisplayServer>(DisplayNodeKey{}, workersProvider,
                                               ui_executor, server);
    }

    virtual int getUnderlyingID() const override
    {
        return server->GetId();
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::SERVER;
    }
    virtual void render(std::unordered_set<DisplayNode*>& selectedNodes,
                        const std::string& filter) override;
    virtual void loadChildren(WorkersProvider*,
                              const boost::asio::any_io_executor&) override
    {
    }
    virtual bool shouldRender(const std::string&) const override
    {
        return true;
    }

    void showInfoDialog();

    ServerPtr server;
    struct UserInfo
    {
        UserInfo(const nlohmann::json& json);
        std::string username;
        std::string password;
        std::string message;
        int auth = -1;
        std::string status;
        std::string expiryDate;
        std::string createdAtDate;
        bool isTrial = false;
        int activeCons = 0; // connections?
        int maxConnections = 0;
        std::vector<std::string> outputFormats;
    };
    struct ServerInfo
    {
        ServerInfo(const nlohmann::json& json);
        std::string url;
        std::string port;
        std::string httpsPort;
        std::string serverProtocol;
        std::string rtmpPort;
        std::string timezone;
        std::string timestampNow;
        std::string timeNow;
        bool process = false;
    };
    std::optional<UserInfo> userInfo;
    std::optional<ServerInfo> serverInfo;
    std::optional<std::string> error;
};
