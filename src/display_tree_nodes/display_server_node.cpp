#include "display_server_node.h"

#include <boost/asio/post.hpp>
#include <boost/url.hpp>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <imgui.h>

#include "../fonts/IconsFontAwesome4.h"
#include "../workers_provider.h"
#include "common.h"
#include "display_server_category.h"

void DisplayServer::render(std::unordered_set<DisplayNode*>& selectedNodes,
                           const std::string& filter)
{
    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selected)
    {
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::SetNextItemOpen(isOpen);
    bool openServerInfo = false;

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::Selectable("Show server info"))
            {
                openServerInfo = true;
                boost::url url;
                url.set_scheme(server->GetUrlScheme());
                url.set_host(server->GetHost());
                url.set_port(server->GetPort());
                url.set_path("/player_api.php");
                auto params = url.params();
                params.set("username", server->GetUsername());
                params.set("password", server->GetPassword());

                workersProvider->GetNetworkResourceProvider()->GetResource(
                    url.buffer(), ui_executor,
                    [weak = weak_from_this()](std::string body, std::error_code ec)
                    {
                        auto selfNode = weak.lock();
                        if (!selfNode)
                            return;
                        auto self =
                            std::static_pointer_cast<DisplayServer>(selfNode);
                        if (ec)
                        {
                            self->error = ec.message();
                            return;
                        }

                        nlohmann::json json = nlohmann::json::parse(body);
                        if (json.contains("user_info") &&
                            json["user_info"].is_object())
                        {
                            self->userInfo.emplace(json["user_info"]);
                        }
                        if (json.contains("server_info") &&
                            json["server_info"].is_object())
                        {
                            self->serverInfo.emplace(json["server_info"]);
                        }
                    },
                    false);
            }
            ImGui::EndPopup();
        }

        isOpen = true;
        for (auto& c : children)
        {
            c->render(selectedNodes, filter);
        }
        ImGui::TreePop();
    }
    else if (ImGui::IsItemToggledOpen())
    {
        isOpen = false;
        clearSelectedChildren(this, selectedNodes);
    }

    if (openServerInfo)
    {
        ImGui::OpenPopup("Server Info");
    }
    showInfoDialog();
}

void DisplayServer::showInfoDialog()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Server Info", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (error)
        {
            ImGui::TextColored({ 1.f, 0.f, 0.f, 1.f }, "Error: %s",
                               error->c_str());
        }
        else if (userInfo || serverInfo)
        {
            ImGui::SeparatorText("User Info");
            if (userInfo)
            {
                ImGui::Text("Username: %s", userInfo->username.c_str());
                ImGui::Text("Password: %s", userInfo->password.c_str());
                ImGui::Text("Message: %s", userInfo->message.c_str());
                ImGui::Text("Auth: %d", userInfo->auth);
                ImGui::Text("Status: %s", userInfo->status.c_str());
                ImGui::Text("Expiry Date: %s", userInfo->expiryDate.c_str());
                ImGui::Text("Created At: %s", userInfo->createdAtDate.c_str());
                ImGui::Text("Is Trial: %s", userInfo->isTrial ? "Yes" : "No");
                ImGui::Text("Active Connections: %d", userInfo->activeCons);
                ImGui::Text("Max Connections: %d", userInfo->maxConnections);
                ImGui::Text("Allowed Output Formats:");
                for (const auto& out : userInfo->outputFormats)
                {
                    ImGui::SameLine();
                    ImGui::Text(" %s", out.c_str());
                }
            }
            ImGui::SeparatorText("Server Info");
            if (serverInfo)
            {
                ImGui::Text("URL: %s", serverInfo->url.c_str());
                ImGui::Text("Port: %s", serverInfo->port.c_str());
                ImGui::Text("HTTPS Port: %s", serverInfo->httpsPort.c_str());
                ImGui::Text("Server Protocol: %s",
                            serverInfo->serverProtocol.c_str());
                ImGui::Text("RTMP Port: %s", serverInfo->rtmpPort.c_str());
                ImGui::Text("Timezone: %s", serverInfo->timezone.c_str());
                ImGui::Text("Time Now: %s", serverInfo->timeNow.c_str());
                ImGui::Text("Timestamp Now: %s",
                            serverInfo->timestampNow.c_str());
            }
        }
        else
        {
            ImGui::TextUnformatted("Loading info...");
        }

        if (ImGui::Button("Close", ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

DisplayServer::UserInfo::UserInfo(const nlohmann::json& json)
{
    if (json.contains("username") && json["username"].is_string())
    {
        username = json["username"].get<std::string>();
    }
    if (json.contains("password") && json["password"].is_string())
    {
        password = json["password"].get<std::string>();
    }
    if (json.contains("message") && json["message"].is_string())
    {
        message = json["message"].get<std::string>();
    }
    if (json.contains("auth") && json["auth"].is_number())
    {
        auth = json["auth"].get<int>();
    }
    if (json.contains("status") && json["status"].is_string())
    {
        status = json["status"].get<std::string>();
    }
    if (json.contains("exp_date"))
    {
        int64_t date = 0;
        if (json["exp_date"].is_number())
        {
            date = json["exp_date"].get<int64_t>();
        }
        else if (json["exp_date"].is_string())
        {
            auto str = json["exp_date"].get<std::string>();
            auto [_, ec] =
                std::from_chars(str.data(), str.data() + str.size(), date);
            if (ec != std::errc{})
            {
                date = 0;
            }
        }
        const std::chrono::system_clock::time_point epoch =
            std::chrono::system_clock::from_time_t(date);

        expiryDate = std::format("{0:%F} {0:%R} {1:}",
                                 std::chrono::current_zone()->to_local(epoch),
                                 std::chrono::current_zone()->name());
    }
    if (json.contains("created_at"))
    {
        int64_t date = 0;
        if (json["created_at"].is_number())
        {
            date = json["created_at"].get<int64_t>();
        }
        else if (json["created_at"].is_string())
        {
            auto str = json["created_at"].get<std::string>();
            auto [_, ec] =
                std::from_chars(str.data(), str.data() + str.size(), date);
            if (ec != std::errc{})
            {
                date = 0;
            }
        }
        const std::chrono::system_clock::time_point epoch =
            std::chrono::system_clock::from_time_t(date);

        createdAtDate = std::format("{0:%F} {0:%R} {1:}",
                                    std::chrono::current_zone()->to_local(epoch),
                                    std::chrono::current_zone()->name());
    }
    if (json.contains("is_trial"))
    {
        if (json["is_trial"].is_boolean())
        {
            isTrial = json["is_trial"].get<bool>();
        }
        else if (json["is_trial"].is_number())
        {
            isTrial = json["is_trial"].get<int>() != 0;
        }
        else if (json["is_trial"].is_string())
        {
            isTrial = json["is_trial"].get<std::string>() != "0" &&
                      json["is_trial"].get<std::string>() != "false";
        }
    }
    if (json.contains("active_cons"))
    {
        if (json["active_cons"].is_number())
        {
            activeCons = json["active_cons"].get<int>();
        }
        else if (json["active_cons"].is_string())
        {
            auto str = json["active_cons"].get<std::string>();
            auto [_, ec] =
                std::from_chars(str.data(), str.data() + str.size(), activeCons);
            if (ec != std::errc{})
            {
                activeCons = 0;
            }
        }
    }
    if (json.contains("max_connections"))
    {
        if (json["max_connections"].is_number())
        {
            maxConnections = json["max_connections"].get<int>();
        }
        else if (json["max_connections"].is_string())
        {
            auto str = json["max_connections"].get<std::string>();
            auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(),
                                           maxConnections);
            if (ec != std::errc{})
            {
                maxConnections = 0;
            }
        }
    }
    if (json.contains("allowed_output_formats") &&
        json["allowed_output_formats"].is_array())
    {
        for (const auto& out : json["allowed_output_formats"])
        {
            outputFormats.push_back(out.get<std::string>());
        }
    }
}
DisplayServer::ServerInfo::ServerInfo(const nlohmann::json& json)
{
    if (json.contains("url") && json["url"].is_string())
    {
        url = json["url"].get<std::string>();
    }
    if (json.contains("port") && json["port"].is_string())
    {
        port = json["port"].get<std::string>();
    }
    if (json.contains("https_port") && json["https_port"].is_string())
    {
        httpsPort = json["https_port"].get<std::string>();
    }
    if (json.contains("server_protocol") && json["server_protocol"].is_string())
    {
        serverProtocol = json["server_protocol"].get<std::string>();
    }
    if (json.contains("rtmp_port") && json["rtmp_port"].is_string())
    {
        rtmpPort = json["rtmp_port"].get<std::string>();
    }
    if (json.contains("timezone") && json["timezone"].is_string())
    {
        timezone = json["timezone"].get<std::string>();
    }
    if (json.contains("time_now") && json["time_now"].is_string())
    {
        timeNow = json["time_now"].get<std::string>();
    }
    if (json.contains("timestamp_now") && json["timestamp_now"].is_number())
    {
        auto timestamp_now = json["timestamp_now"].get<int64_t>();
        const std::chrono::system_clock::time_point epoch =
            std::chrono::system_clock::from_time_t(timestamp_now);

        try
        {
            auto chronotimezone = std::chrono::locate_zone(timezone);
            timestampNow = std::format("{0:%F} {0:%R} {1:}",
                                       chronotimezone->to_local(epoch),
                                       chronotimezone->name());
        }
        catch (const std::exception& ex)
        {
        }
    }
}

DisplayServer::DisplayServer(DisplayNodeKey key,
                             WorkersProvider* workersProvider,
                             const boost::asio::any_io_executor& ui_executor,
                             ServerPtr server)
: DisplayChannelsGroup{ key,
                        reinterpret_cast<const char*>(ICON_FA_SERVER " ") +
                            server->GetHost(),
                        workersProvider, ui_executor, nullptr }
, server{ server }
{
    boost::url url;
    url.set_scheme(server->GetUrlScheme());
    url.set_host(server->GetHost());
    url.set_port(server->GetPort());
    url.set_path("/player_api.php");
    auto params = url.params();
    params.set("username", server->GetUsername());
    params.set("password", server->GetPassword());
    params.set("action", "get_live_categories");

    auto liveCategory = DisplayServerCategory::Create(
        reinterpret_cast<const char*>(ICON_FA_TELEVISION " Live"), url.buffer(),
        workersProvider, ui_executor, this);
    liveCategory->reloadLocalChannelsSignal.connect(
        std::ref(reloadLocalChannelsSignal));
    children.push_back(std::move(liveCategory));
    params.replace(params.find("action"), { "action", "get_vod_categories" });
    auto vodCategory = DisplayServerCategory::Create(
        reinterpret_cast<const char*>(ICON_FA_VIDEO_CAMERA " VODs"),
        url.buffer(), workersProvider, ui_executor, this);
    vodCategory->reloadLocalChannelsSignal.connect(
        std::ref(reloadLocalChannelsSignal));
    children.push_back(std::move(vodCategory));
}
