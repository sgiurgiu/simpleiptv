#include "display_node.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "fonts/IconsFontAwesome4.h"

namespace
{
void clearSelectedNodes(std::unordered_set<DisplayNode*>& selectedNodes)
{
    for (DisplayNode* node : selectedNodes)
    {
        node->selected = false;
    }
    selectedNodes.clear();
}
void clearSelectedChildren(DisplayNode* node,
                           std::unordered_set<DisplayNode*>& selectedNodes)
{
    node->selected = false;
    selectedNodes.erase(node);
    for (auto& node : node->children)
    {
        clearSelectedChildren(node.get(), selectedNodes);
    }
}

} // namespace

void DisplayRootChannelsGroup::renderGroup(
    std::unordered_set<DisplayNode*>& selectedNodes)
{
    loadChildren();
    for (auto& g : children)
    {
        g->render(selectedNodes);
    }
}

DisplayNode* DisplayNode::getNextNode()
{
    DisplayNode* curr_node = this;
    curr_node->loadChildren();
    if (!curr_node->children.empty())
    {
        isOpen = true;
        return curr_node->children.begin()->get();
    }
    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren();
        if (curr_node->indexInParent + 1 < (int)curr_node->parent->children.size())
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent + 1).get();
            node->isOpen = true;
            if (!node->children.empty())
            {
                return node->children.begin()->get();
            }
            else
            {
                return node;
            }
        }
        curr_node = curr_node->parent;
    }
    return nullptr;
}

DisplayNode* DisplayNode::getPreviousNode()
{
    DisplayNode* curr_node = this;

    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren();
        if (curr_node->indexInParent > 0)
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent - 1).get();
            node->loadChildren();
            node->isOpen = true;
            if (!node->children.empty())
            {
                return node->children.rbegin()->get();
            }
            else
            {
                return node;
            }
        }
        curr_node = curr_node->parent;
    }
    return nullptr;
}

void DisplayRootChannelsGroup::setRoot(RootChannelsGroupPtr root)
{
    this->root = root;
    this->group = root;
    favouritesGroup = std::make_unique<DisplayFavouritesChannelsGroup>(this);
    root->IterateFavouriteChannels(
        [this](ChannelPtr channel)
        {
            auto dchannel =
                std::make_unique<DisplayChannel>(channel, favouritesGroup.get());
            dchannel->indexInParent = favouritesGroup->children.size();
            favouritesGroup->children.push_back(std::move(dchannel));
        });
    favouritesGroup->indexInParent = 0;
    children.push_back(std::move(favouritesGroup));
}

void DisplayRootChannelsGroup::loadChildren()
{
    if (!root || !root->AreGroupsLoaded())
    {
        return;
    }

    if (children.size() < 2 && root->AreGroupsLoaded())
    {
        root->IterateGroups(
            [this](ChannelsGroupPtr group)
            {
                children.emplace_back(
                    std::make_unique<DisplayChannelsGroup>(group, this));
                children.rbegin()->get()->indexInParent = children.size() - 1;
            });
    }
}

void DisplayChannel::renderChannel(std::unordered_set<DisplayNode*>& selectedNodes)
{
    const bool isSelected = selected;

    ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
    if (isActivated && !isSelected)
    {
        flags |= ImGuiSelectableFlags_Highlight;

        ImGui::PushStyleColor(
            ImGuiCol_HeaderHovered,
            ImVec4(103.f / 255.f, 135.f / 255.f, 104.f / 255.f, 1.f));
    }

    if (ImGui::Selectable(channel->GetName().c_str(), isSelected, flags))
    {
        selected = !selected;
        if (ImGui::GetIO().KeyCtrl)
        {
            // just this item changed selection
            if (selected)
            {
                selectedNodes.insert(this);
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
        else if (ImGui::GetIO().KeyShift)
        {
            // TODO: this is tricky
            if (selected)
            {
                selectedNodes.insert(this);
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
        else
        {
            // if we were selected, and toggled
            if (!selected && !selectedNodes.empty())
                selected = true;

            if (selected)
            {
                clearSelectedNodes(selectedNodes);
                selectedNodes.insert(this);
                selected = true;
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
    }
    if (isActivated && !isSelected)
    {
        ImGui::PopStyleColor(1);
    }

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        activatedChannelSignal(this);
        isActivated = true;
    }
}
void DisplayChannelsGroup::loadChildren()
{
    if (children.empty() && group)
    {
        group->IterateChannels(
            [this](auto& channel)
            {
                children.emplace_back(
                    std::make_unique<DisplayChannel>(channel, this));
                children.rbegin()->get()->indexInParent = children.size() - 1;
            });
    }
}
void DisplayChannelsGroup::renderGroup(std::unordered_set<DisplayNode*>& selectedNodes)
{
    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selected)
    {
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (openByDefault)
    {
        //        tree_node_flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    ImGui::SetNextItemOpen(isOpen);

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        if (group && !group->AreChannelsLoaded())
        {
            ImGui::Text("Loading...");
        }
        else
        {
            isOpen = true;
            loadChildren();
            for (auto& c : children)
            {
                c->render(selectedNodes);
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
DisplayNode::DisplayNode() : DisplayNode{ nullptr }
{
}
DisplayNode::DisplayNode(DisplayNode* parent) : DisplayNode{ "", parent }
{
}
DisplayNode::DisplayNode(const std::string& name, DisplayNode* parent)
: parent{ parent }, name{ name }
{
    if (parent)
    {
        activatedChannelSignal.connect(parent->activatedChannelSignal);
    }
}
DisplayFavouritesChannelsGroup::DisplayFavouritesChannelsGroup(
    DisplayRootChannelsGroup* parent)
: DisplayChannelsGroup{
    reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"), parent
}
{
    openByDefault = true;
    isOpen = true;
}
