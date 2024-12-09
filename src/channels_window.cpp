#include "channels_window.h"

#include <boost/asio/post.hpp>
#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include "fonts/IconsFontAwesome4.h"

namespace
{
constexpr float MAX_TIMEOUT = 10.f;
constexpr float INITIAL_BG_ALPHA = 0.5f;
static ImGuiSelectionBasicStorage localTreeSelection;
static ImGuiID GlobalTreeNodeUid = 0;
} // namespace

std::shared_ptr<ChannelsWindow>
ChannelsWindow::Create(const boost::asio::any_io_executor& executor,
                       WorkersProvider& workersProvider)
{
    auto window =
        std::make_shared<ChannelsWindow>(Key{}, executor, workersProvider);
    window->loadLocalChannels();
    return window;
}

ChannelsWindow::ChannelsWindow(Key,
                               const boost::asio::any_io_executor& ui_executor,
                               WorkersProvider& workersProvider)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, bgAlpha{ INITIAL_BG_ALPHA }
{
}

void ChannelsWindow::showWindow(bool forceDisplay)
{
    if (!forceDisplay)
    {
        if (ImGui::GetCurrentContext()->MouseStationaryTimer >= MAX_TIMEOUT)
        {
            bgAlpha = INITIAL_BG_ALPHA;
            return;
        }

        if (ImGui::GetCurrentContext()->MouseStationaryTimer > (MAX_TIMEOUT / 3.f))
        {
            bgAlpha = (-INITIAL_BG_ALPHA / MAX_TIMEOUT) *
                          ImGui::GetCurrentContext()->MouseStationaryTimer +
                      INITIAL_BG_ALPHA;
        }
        else
        {
            bgAlpha = INITIAL_BG_ALPHA;
        }
    }
    else
    {
        bgAlpha = INITIAL_BG_ALPHA;
    }

    auto mainViewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(
        ImVec2(mainViewport->WorkPos.x, mainViewport->WorkPos.y));
    ImGui::SetNextWindowSize(
        ImVec2(mainViewport->WorkSize.x * 0.2f,
               mainViewport->WorkSize.y - ImGui::GetStyle().WindowBorderSize),
        ImGuiCond_None);
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    if (!ImGui::Begin("Channels", nullptr,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    showMenu();

    if (ImGui::BeginTabBar("ChannelsTabBar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Local"))
        {
            showLocalChannelsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Remote"))
        {
            showRemoteChannelsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ChannelsWindow::showLocalChannelsTab()
{
    if (ImGui::InputTextWithHint("##filterChannels", "Filter", &channelsFilter))
    {
    }
    ImGui::BeginChild("##localChannelsTab", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    rootNode.render();
    ImGui::EndChild();
}
void ChannelsWindow::showRemoteChannelsTab()
{
}

void ChannelsWindow::loadLocalChannels()
{
    auto start = std::chrono::high_resolution_clock::now();
    spdlog::debug("starting to load channels");
    workersProvider.GetChannelsRepository()->LoadChannelsAndGroups(
        [weak = weak_from_this(), start](RootChannelsGroupPtr root)
        {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = end - start;
            spdlog::debug(
                "done loading channels. duration: {} ms",
                std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                    .count());
            auto self = weak.lock();
            if (!self)
                return;

            self->rootNode.setRoot(root);
        },
        ui_executor);
}

void ChannelsWindow::showMenu()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Add Server"))
            {
            }
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q", &quit);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void ChannelsWindow::DisplayRootChannelsGroup::renderGroup()
{
    if (!root || !root->AreGroupsLoaded())
    {
        return;
    }

    ImGuiMultiSelectFlags flags =
        ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect1d;
    ImGuiMultiSelectIO* ms_io =
        ImGui::BeginMultiSelect(flags, localTreeSelection.Size, -1);
    applySelectionRequests(ms_io, &localTreeSelection);

    if (children.size() < 2 && root->AreGroupsLoaded())
    {
        root->IterateGroups(
            [this](ChannelsGroupPtr group)
            {
                children.emplace_back(
                    std::make_unique<DisplayChannelsGroup>(group, this));
            });
    }
    for (auto& g : children)
    {
        g->render();
    }

    ms_io = ImGui::EndMultiSelect();
    applySelectionRequests(ms_io, &localTreeSelection);
}

void ChannelsWindow::DisplayRootChannelsGroup::applySelectionRequests(
    ImGuiMultiSelectIO* ms_io, ImGuiSelectionBasicStorage* selection)
{
    for (ImGuiSelectionRequest& req : ms_io->Requests)
    {
        if (req.Type == ImGuiSelectionRequestType_SetAll)
        {
            if (req.Selected)
                setNodesSelection(selection, req.Selected);
            else
                selection->Clear();
        }
        else if (req.Type == ImGuiSelectionRequestType_SetRange)
        {
            DisplayNode* first_node = (DisplayNode*)(uintptr_t)req.RangeFirstItem;
            DisplayNode* last_node = (DisplayNode*)(uintptr_t)req.RangeLastItem;
            for (DisplayNode* node = first_node; node != NULL;
                 node = getNextNode())
            {
                selection->SetItemSelected(node->uid, req.Selected);
                if (node == last_node)
                {
                    break;
                }
            }
        }
    }
}

ChannelsWindow::DisplayNode* ChannelsWindow::DisplayNode::getNextNode()
{
    if (ImGui::GetStateStorage()->GetBool(uid))
    {
        if (!children.empty())
        {
            return children.at(0).get();
        }
    }
    DisplayNode* curr_node = this;
    while (curr_node->parent != nullptr)
    {
        if (curr_node->indexInParent + 1 < (int)curr_node->parent->children.size())
        {
            return curr_node->parent->children.at(curr_node->indexInParent + 1).get();
        }
        curr_node = curr_node->parent;
    }
    return nullptr;
}

void ChannelsWindow::DisplayNode::setNodesSelection(
    ImGuiSelectionBasicStorage* selection, bool selected)
{
    if (parent != nullptr) // Root node isn't visible nor selectable in our scheme
        selection->SetItemSelected((ImGuiID)uid, selected);

    if (parent == nullptr || ImGui::GetStateStorage()->GetBool(uid))
    {
        for (const auto& child : children)
        {
            child->setNodesSelection(selection, selected);
        }
    }
}

void ChannelsWindow::DisplayRootChannelsGroup::setRoot(RootChannelsGroupPtr root)
{
    this->root = root;
    this->group = root;
    favouritesGroup = std::make_unique<DisplayFavouritesChannelsGroup>(this);
    root->IterateFavouriteChannels(
        [this](ChannelPtr channel)
        {
            favouritesGroup->children.emplace_back(
                std::make_unique<DisplayChannel>(channel, this));
        });
    children.push_back(std::move(favouritesGroup));
}

void ChannelsWindow::DisplayChannel::renderChannel()
{
    bool item_is_selected = localTreeSelection.Contains(uid);
    ImGui::SetNextItemSelectionUserData(
        (ImGuiSelectionUserData)(uintptr_t)pointer);
    ImGui::SetNextItemStorageID(uid);

    ImGui::Selectable(channel->GetName().c_str(), item_is_selected);
}

void ChannelsWindow::DisplayFavouritesChannelsGroup::renderGroup()
{
    bool item_is_selected = localTreeSelection.Contains(uid);
    ImGui::SetNextItemSelectionUserData(
        (ImGuiSelectionUserData)(uintptr_t)pointer);
    ImGui::SetNextItemStorageID(uid);
    ImGuiTreeNodeFlags tree_node_flags =
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
    if (item_is_selected)
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        for (auto& c : children)
        {
            c->render();
        }
        ImGui::TreePop();
    }
    else if (ImGui::IsItemToggledOpen())
    {
        closeAndUnselectChildNodes(&localTreeSelection);
    }
}
int ChannelsWindow::DisplayNode::closeAndUnselectChildNodes(
    ImGuiSelectionBasicStorage* selection, int depth)
{
    int unselected_count = selection->Contains((ImGuiID)uid) ? 1 : 0;
    if (depth == 0 || ImGui::GetStateStorage()->GetBool(uid))
    {
        for (auto& child : children)
            unselected_count +=
                child->closeAndUnselectChildNodes(selection, depth + 1);
        ImGui::GetStateStorage()->SetBool(uid, false);
    }

    // Select root node if any of its child was selected, otherwise unselect
    selection->SetItemSelected(uid, (depth == 0 && unselected_count > 0));
    return unselected_count;
}
void ChannelsWindow::DisplayChannelsGroup::renderGroup()
{
    bool item_is_selected = localTreeSelection.Contains(uid);
    ImGui::SetNextItemSelectionUserData(
        (ImGuiSelectionUserData)(uintptr_t)pointer);
    ImGui::SetNextItemStorageID(uid);
    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (item_is_selected)
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        if (!group->AreChannelsLoaded())
        {
            ImGui::Text("Loading...");
        }
        else
        {
            if (children.empty())
            {
                group->IterateChannels(
                    [this](auto& channel)
                    {
                        children.emplace_back(
                            std::make_unique<DisplayChannel>(channel, this));
                    });
            }
            for (auto& c : children)
            {
                c->render();
            }
        }
        ImGui::TreePop();
    }
    else if (ImGui::IsItemToggledOpen())
    {
        closeAndUnselectChildNodes(&localTreeSelection);
    }
}
ChannelsWindow::DisplayNode::DisplayNode() : DisplayNode{ nullptr }
{
}
ChannelsWindow::DisplayNode::DisplayNode(DisplayNode* parent)
: DisplayNode{ "", parent }
{
}
ChannelsWindow::DisplayNode::DisplayNode(const std::string& name,
                                         DisplayNode* parent)
: uid{ GlobalTreeNodeUid++ }, parent{ parent }, name{ name }
{
    pointer = reinterpret_cast<uintptr_t>(this);
}
ChannelsWindow::DisplayFavouritesChannelsGroup::DisplayFavouritesChannelsGroup(
    DisplayRootChannelsGroup* parent)
: DisplayChannelsGroup{
    reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"), parent
}
{
}