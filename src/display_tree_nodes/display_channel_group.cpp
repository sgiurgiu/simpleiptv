#include "display_channel_group.h"

#include "common.h"
#include "display_channel.h"
#include <imgui.h>

void DisplayChannelsGroup::loadChildren(WorkersProvider* workersProvider,
                                        const boost::asio::any_io_executor& ui_executor)
{
    if (children.empty() && group)
    {
        group->IterateChannels(
            [this, workersProvider, ui_executor](auto& channel)
            {
                auto dchannel = DisplayChannel::Create(channel, workersProvider,
                                                       ui_executor, this);
                children.emplace_back(dchannel);
                dchannel->loadLogo();
                dchannel->indexInParent = children.size() - 1;
            });
    }
}
bool DisplayChannelsGroup::shouldRender(const std::string& filter) const
{
    if (filter.empty())
        return true;
    bool shouldRender = false;
    for (auto& c : children)
    {
        shouldRender |= c->shouldRender(filter);
    }
    return shouldRender;
}
void DisplayChannelsGroup::renderGroup(
    std::unordered_set<DisplayNode*>& selectedNodes, const std::string& filter)
{
    if (!shouldRender(filter))
        return;

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
        if (group && !group->AreChannelsLoaded())
        {
            ImGui::Text("Loading...");
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

    if (ImGui::IsItemHovered())
    {
    }
}

void DisplayChannelsGroup::removeChannel(std::shared_ptr<DisplayChannel> channel)
{
    if (!channel)
        return;
    if (group)
    {
        group->RemoveChannel(channel->channel->GetId());
    }
    std::erase(children, channel);
}
void DisplayChannelsGroup::addChannel(ChannelPtr channel)
{
    if (group)
    {
        group->AddChannel(channel);
    }

    auto dchannel =
        DisplayChannel::Create(channel, workersProvider, ui_executor, this);
    children.emplace_back(dchannel);
    dchannel->loadLogo();
    dchannel->indexInParent = children.size() - 1;
}
