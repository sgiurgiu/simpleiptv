#pragma once

#include <memory>
#include <string>
#include <vector>

class Server
{
public:
    Server(int id,
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
           std::vector<std::string> outputFormats);

    int GetId() const
    {
        return id;
    }
    const std::string& GetHost() const
    {
        return host;
    }
    const std::string& GetPort() const
    {
        return port;
    }
    const std::string& GetUrlScheme() const
    {
        return urlScheme;
    }
    const std::string& GetUsername() const
    {
        return username;
    }
    const std::string& GetPassword() const
    {
        return password;
    }
    const std::string& GetTimezone() const
    {
        return timezone;
    }
    const std::string& GetStatus() const
    {
        return status;
    }
    int GetExpiryDate() const
    {
        return expiryDate;
    }
    int GetCreatedAt() const
    {
        return createdAt;
    }
    bool IsTrial() const
    {
        return trial;
    }
    int GetMaxConnections() const
    {
        return maxConnections;
    }
    const std::string& GetRTMPPort() const
    {
        return rtmpPort;
    }
    const std::string& GetHTTPSPort() const
    {
        return httpsPort;
    }
    const std::vector<std::string>& GetOutputFormats() const
    {
        return outputFormats;
    }

private:
    int id = 0;
    std::string host;
    std::string port;
    std::string urlScheme;
    std::string username;
    std::string password;
    std::string timezone;
    std::string status;
    int expiryDate = 0;
    int createdAt = 0;
    bool trial = false;
    int maxConnections = 0;
    std::string rtmpPort;
    std::string httpsPort;
    std::vector<std::string> outputFormats;
};
using ServerPtr = std::shared_ptr<Server>;