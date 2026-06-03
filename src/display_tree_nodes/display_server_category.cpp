#include "display_server_category.h"

#include <boost/url.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>
#include <utility>

#include "../fonts/IconsFontAwesome4.h"
#include "../workers_provider.h"
#include "common.h"
#include "display_remote_channel_group.h"

DisplayServerCategory::DisplayServerCategory(
    DisplayNodeKey key,
    const std::string& name,
    const std::string& url,
    WorkersProvider* workersProvider,
    const boost::asio::any_io_executor& ui_executor,
    DisplayServer* parent)
: DisplayChannelsGroup{ key, name, workersProvider, ui_executor, parent }
, displayServer{ parent }
, url{ url }
, groupsFilterLabel{ fmt::format("##filterRemoteGroups{}", name) }
, eraserLabel{ fmt::format("{}##eraserGroupsFilter{}",
                           reinterpret_cast<const char*>(ICON_FA_ERASER),
                           name) }
{
}

void DisplayServerCategory::render(std::unordered_set<DisplayNode*>& selectedNodes,
                                   const std::string&)
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
            ImGui::InputTextWithHint(groupsFilterLabel.c_str(), "Filter",
                                     &groupsFilter);
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
            if (ImGui::Button(eraserLabel.c_str()))
            {
                groupsFilter.clear();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            for (auto& c : children)
            {
                c->render(selectedNodes, groupsFilter);
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
                auto group = DisplayRemoteChannelsGroup::Create(
                    catName, url.buffer(), self->workersProvider,
                    self->ui_executor, self.get());
                group->reloadLocalChannelsSignal.connect(
                    std::ref(self->reloadLocalChannelsSignal));
                self->children.push_back(std::move(group));
            }
        },
        false);
}
