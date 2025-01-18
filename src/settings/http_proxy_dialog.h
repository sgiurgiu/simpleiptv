#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <imgui.h>
#include <memory>

#include "../workers_provider.h"

class HTTPProxyDialog : public std::enable_shared_from_this<HTTPProxyDialog>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    HTTPProxyDialog(Key,
                    boost::asio::any_io_executor executor,
                    WorkersProvider* workersProvider);
    static std::shared_ptr<HTTPProxyDialog> Create(
        boost::asio::any_io_executor executor, WorkersProvider* workersProvider);
    void ShowDialog();
    void SetShowHTTPProxyDialog(bool flag)
    {
        showingDialog = flag;
    }

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    bool showingDialog = false;
    char proxyHostname[1024] = { 0 };
    int proxyPort = { 0 };
    bool useProxy = false;
    HttpProxy proxy;
};