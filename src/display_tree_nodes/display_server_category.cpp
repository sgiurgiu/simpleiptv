#include "display_server_category.h"

#include <boost/url.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "../workers_provider.h"
#include "common.h"
#include "display_remote_channel_group.h"

void DisplayServerCategory::render(std::unordered_set<DisplayNode*>& selectedNodes,
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
void DisplayServerCategory::loadRemoteChildren()
{
    spdlog::debug("DisplayServerCategory - loading remote children from {}", url);
    workersProvider->GetNetworkResourceProvider()->GetResource(
        url, ui_executor,
        [weak = weak_from_this()](std::string body, std::error_code ec)
        {
            auto selfNode = weak.lock();
            if (!selfNode)
                return;
            auto self = std::static_pointer_cast<DisplayServerCategory>(selfNode);
            if (ec)
            {
                return;
            }

            nlohmann::json json = nlohmann::json::parse(body);
            for (const auto& cat : json)
            {
                auto catId = cat["category_id"].get<std::string>();
                auto catName = cat["category_name"].get<std::string>();
                boost::url url = boost::urls::parse_uri(self->url).value();
                auto params = url.params();
                auto actionIt = params.find("action");
                std::string action = "get_live_streams";
                if ((*actionIt)->value == "get_vod_categories")
                {
                    action = "get_vod_streams";
                }
                params.replace(actionIt, { "action", action });
                params.set("category_id", catId);
                self->children.push_back(DisplayRemoteChannelsGroup::Create(
                    catName, url.buffer(), self->workersProvider,
                    self->ui_executor, self.get()));
            }
        },
        false);
}
