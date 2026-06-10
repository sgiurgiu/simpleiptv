#include "server_dialog.h"

#include <array>
#include <chrono>
#include <imgui_stdlib.h>

namespace
{
constexpr std::string_view LABEL_TEMPLATE = "Username:X";
constexpr std::string_view FIELD_TEMPLATE = "XXXXXXXXXXXXXXXXXXXXXX";
constexpr std::array<const char*, 2> SCHEMAS = { "http", "https" };

constexpr const char* POPUP_ID = "Server";
constexpr const char* REMOVE_POPUP_ID = "Remove Server";
} // namespace

ServerDialog::ServerDialog(Key,
                           boost::asio::any_io_executor executor,
                           WorkersProvider* workersProvider)
: ui_executor{ executor }, workersProvider{ workersProvider }
{
}

std::shared_ptr<ServerDialog> ServerDialog::Create(
    boost::asio::any_io_executor executor, WorkersProvider* workersProvider)
{
    return std::make_shared<ServerDialog>(Key{}, executor, workersProvider);
}

void ServerDialog::SetShowAddServerDialog()
{
    editMode = false;
    editingServerId = 0;
    host.clear();
    port.clear();
    schema = 0;
    username.clear();
    password.clear();
    showingDialog = true;
}

void ServerDialog::SetShowEditServerDialog(ServerPtr server)
{
    editMode = true;
    editingServerId = server->GetId();
    host = server->GetHost();
    port = server->GetPort();
    schema = server->GetUrlScheme() == "https" ? 1 : 0;
    username = server->GetUsername();
    password = server->GetPassword();
    showingDialog = true;
}

void ServerDialog::saveServer()
{
    if (editMode)
    {
        Server server{ editingServerId,
                       host,
                       port,
                       SCHEMAS[schema],
                       username,
                       password,
                       "",
                       "",
                       0,
                       0,
                       false,
                       0,
                       "",
                       "",
                       {} };
        workersProvider->GetServersRepository()->UpdateServer(
            server,
            [weak = weak_from_this()]()
            {
                auto self = weak.lock();
                if (!self)
                    return;
                self->serversChangedSignal();
            },
            ui_executor);
    }
    else
    {
        auto createdAt = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        Server server{ 0,
                       host,
                       port,
                       SCHEMAS[schema],
                       username,
                       password,
                       "",
                       "",
                       0,
                       static_cast<int>(createdAt),
                       false,
                       0,
                       "",
                       "",
                       {} };
        workersProvider->GetServersRepository()->AddServer(
            server,
            [weak = weak_from_this()](ServerPtr)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                self->serversChangedSignal();
            },
            ui_executor);
    }
}

void ServerDialog::SetShowRemoveServerDialog(ServerPtr server)
{
    removingServerId = server->GetId();
    removingServerHost = server->GetHost();
    showingRemoveDialog = true;
}

void ServerDialog::removeServer()
{
    workersProvider->GetServersRepository()->RemoveServer(
        removingServerId,
        [weak = weak_from_this()]()
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->serversChangedSignal();
        },
        ui_executor);
}

void ServerDialog::ShowDialog()
{
    showServerDialog();
    showRemoveServerDialog();
}

void ServerDialog::showServerDialog()
{
    if (showingDialog)
    {
        ImGui::OpenPopup(POPUP_ID);
        showingDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(POPUP_ID, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SeparatorText(editMode ? "Edit Server" : "Add Server");

        auto fieldsPosition = ImGui::CalcTextSize(LABEL_TEMPLATE.data()).x;
        auto fieldWidth = ImGui::CalcTextSize(FIELD_TEMPLATE.data()).x;

        ImGui::Text("Host:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##server_host", &host);
        ImGui::PopItemWidth();

        ImGui::Text("Port:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##server_port", &port);
        ImGui::PopItemWidth();

        ImGui::Text("Schema:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::Combo("##server_schema", &schema, SCHEMAS.data(),
                     static_cast<int>(SCHEMAS.size()));
        ImGui::PopItemWidth();

        ImGui::Text("Username:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##server_username", &username);
        ImGui::PopItemWidth();

        ImGui::Text("Password:");
        ImGui::SameLine(fieldsPosition);
        ImGui::PushItemWidth(fieldWidth);
        ImGui::InputText("##server_password", &password,
                         ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            saveServer();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ServerDialog::showRemoveServerDialog()
{
    if (showingRemoveDialog)
    {
        ImGui::OpenPopup(REMOVE_POPUP_ID);
        showingRemoveDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(REMOVE_POPUP_ID, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to remove the server\n'%s'?",
                    removingServerHost.c_str());
        ImGui::TextDisabled("Its cached channels and EPG data will be deleted.");

        ImGui::Separator();
        if (ImGui::Button("Remove", ImVec2(120, 0)))
        {
            removeServer();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
