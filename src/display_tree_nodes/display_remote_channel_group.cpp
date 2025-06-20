#include "display_remote_channel_group.h"

#include <boost/url.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "../workers_provider.h"
#include "common.h"
#include "display_channel.h"

void DisplayRemoteChannelsGroup::render(
    std::unordered_set<DisplayNode*>& selectedNodes, const std::string& filter)
{
    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selected)
    {
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::SetNextItemOpen(isOpen);

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        isOpen = true;
        if (children.empty())
        {
            ImGui::Text("Loading...");
            if (!areChildrenLoading)
            {
                areChildrenLoading = true;
                loadRemoteChildren();
            }
        }
        else
        {
            for (auto& c : children)
            {
                c->render(selectedNodes, filter);
            }
        }
        ImGui::TreePop();
    }
    else if (ImGui::IsItemToggledOpen())
    {
        isOpen = false;
        clearSelectedChildren(this, selectedNodes);
    }
}
void DisplayRemoteChannelsGroup::loadRemoteChildren()
{
    spdlog::debug(
        "DisplayRemoteChannelsGroup - loading remote children from {}", url);
    workersProvider->GetNetworkResourceProvider()->GetResource(
        url, ui_executor,
        [weak = weak_from_this()](std::string body, std::error_code ec)
        {
            auto selfNode = weak.lock();
            if (!selfNode)
                return;
            auto self =
                std::static_pointer_cast<DisplayRemoteChannelsGroup>(selfNode);
            if (ec)
            {
                return;
            }

            nlohmann::json json = nlohmann::json::parse(body);
            for (const auto& ch : json)
            {
                auto streamId = ch["stream_id"].get<int>();
                auto streamType = ch["stream_type"].get<std::string>();
                auto name = ch["name"].get<std::string>();
                auto icon = ch["stream_icon"].get<std::string>();

                boost::url channelUrl = boost::urls::parse_uri(self->url).value();
                boost::url epgUrl = channelUrl;

                auto channelParams = channelUrl.params();
                auto username = (*channelParams.find("username"))->value;
                auto password = (*channelParams.find("password"))->value;
                channelParams.clear();

                channelUrl.set_path(std::format("/{}/{}/{}/{}.ts", streamType,
                                                username, password, streamId));
                auto epgParams = epgUrl.params();
                epgParams.replace(epgParams.find("action"),
                                  { "action", "get_short_epg" });
                epgParams.set("stream_id", std::to_string(streamId));

                auto channel = std::make_shared<Channel>(
                    -1, name, channelUrl.buffer(), icon, "", epgUrl.buffer(),
                    "", -1, false, -1);

                self->children.push_back(
                    DisplayChannel::Create(channel, self->workersProvider,
                                           self->ui_executor, self.get()));
            }
        },
        false);
}
