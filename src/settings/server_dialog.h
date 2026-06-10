#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <imgui.h>
#include <memory>
#include <string>

#include "../servers/server.h"
#include "../workers_provider.h"

class ServerDialog : public std::enable_shared_from_this<ServerDialog>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ServerDialog(Key,
                 boost::asio::any_io_executor executor,
                 WorkersProvider* workersProvider);
    static std::shared_ptr<ServerDialog> Create(
        boost::asio::any_io_executor executor, WorkersProvider* workersProvider);
    void ShowDialog();
    void SetShowAddServerDialog();
    void SetShowEditServerDialog(ServerPtr server);
    void SetShowRemoveServerDialog(ServerPtr server);

    // notified (on the ui executor) whenever a server is added or updated
    template <typename S>
    void AddServersChangedListener(S slot)
    {
        serversChangedSignal.connect(slot);
    }

private:
    void saveServer();
    void removeServer();
    void showServerDialog();
    void showRemoveServerDialog();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    bool showingDialog = false;
    bool editMode = false;
    int editingServerId = 0;
    std::string host;
    std::string port;
    int schema = 0; // index into schemas
    std::string username;
    std::string password;
    bool showingRemoveDialog = false;
    int removingServerId = 0;
    std::string removingServerHost;
    boost::signals2::signal<void()> serversChangedSignal;
};
