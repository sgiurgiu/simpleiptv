#include "display_node.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include <algorithm>
#include <boost/asio/post.hpp>

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
    std::unordered_set<DisplayNode*>& selectedNodes, const std::string& filter)
{
    // loadChildren();
    for (auto& g : children)
    {
        g->render(selectedNodes, filter);
    }
}

DisplayNode* DisplayNode::getNextNode(const boost::asio::any_io_executor& executor)
{
    DisplayNode* curr_node = this;
    curr_node->loadChildren(executor);
    if (!curr_node->children.empty())
    {
        isOpen = true;
        return curr_node->children.begin()->get();
    }
    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(executor);
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

DisplayNode* DisplayNode::getPreviousNode(const boost::asio::any_io_executor& executor)
{
    DisplayNode* curr_node = this;

    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(executor);
        if (curr_node->indexInParent > 0)
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent - 1).get();
            node->loadChildren(executor);
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

void DisplayRootChannelsGroup::setRoot(RootChannelsGroupPtr root,
                                       const boost::asio::any_io_executor& executor)
{
    children.clear();
    this->root = root;
    this->group = root;
    favouritesGroup = std::make_unique<DisplayFavouritesChannelsGroup>(this);
    root->IterateFavouriteChannels(
        [this, &executor](ChannelPtr channel)
        {
            auto dchannel = std::make_unique<DisplayChannel>(
                channel, favouritesGroup.get(), executor);
            dchannel->indexInParent = favouritesGroup->children.size();
            favouritesGroup->children.push_back(std::move(dchannel));
        });
    favouritesGroup->indexInParent = 0;
    children.push_back(std::move(favouritesGroup));
    loadChildren(executor);
}

void DisplayRootChannelsGroup::loadChildren(
    const boost::asio::any_io_executor& executor)
{
    if (!root || !root->AreGroupsLoaded())
    {
        return;
    }

    if (children.size() < 2 && root->AreGroupsLoaded())
    {
        root->IterateGroups(
            [this, &executor](ChannelsGroupPtr group)
            {
                children.emplace_back(
                    std::make_unique<DisplayChannelsGroup>(group, this));
                children.back().get()->indexInParent = children.size() - 1;
                children.back().get()->loadChildren(executor);
            });
    }
}

bool DisplayChannel::shouldRender(const std::string& filter) const
{
    if (filter.empty())
        return true;
    auto it = std::search(name.begin(), name.end(), filter.begin(),
                          filter.end(), [](char c1, char c2)
                          { return std::tolower(c1) == std::tolower(c2); });
    return it != name.end();
}

void DisplayChannel::renderChannel(std::unordered_set<DisplayNode*>& selectedNodes,
                                   const std::string& filter)
{
    if (!shouldRender(filter))
    {
        return;
    }

    const bool isSelected = selected;

    ImGuiSelectableFlags flags =
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
    if (isActivated && !isSelected)
    {
        flags |= ImGuiSelectableFlags_Highlight;

        ImGui::PushStyleColor(
            ImGuiCol_HeaderHovered,
            ImVec4(103.f / 255.f, 135.f / 255.f, 104.f / 255.f, 1.f));
    }
    ImGui::PushID(this);
    if (ImGui::Selectable("##channel", isSelected, flags))
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
            // TODO: this is tricky, so leave it for later
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
    ImGui::PopID();
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
    ImGui::SameLine();
    loadLogoTexture();
    if (channelLogoTexture)
    {
        ImVec2 size = displayLogoSize;
        ImVec2 dummySize{ ImGui::GetStyle().ItemSpacing.x, size.y };
        if (size.x < parent->maxLogoWidth)
        {
            dummySize.x += parent->maxLogoWidth - size.x;
        }
        ImTextureID texture = static_cast<ImTextureID>(channelLogoTexture);
        ImGui::Image(texture, displayLogoSize);
        ImGui::SameLine(0.f, dummySize.x);
    }
    ImGui::Text("%s", channel->GetName().c_str());
}
void DisplayChannel::loadLogoTexture()
{
    if (logoData && !channelLogoTexture)
    {
        glGenTextures(1, &channelLogoTexture);
        glBindTexture(GL_TEXTURE_2D, channelLogoTexture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE); // This is required on WebGL
                                           // for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE); // Same
        if (logoChannels == 3)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, logoWidth, logoHeight, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, logoData);
        }
        else if (logoChannels == 4)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logoWidth, logoHeight, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, logoData);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        ImVec2 size = displayLogoSize;
        if (parent->maxLogoWidth < size.x)
        {
            parent->maxLogoWidth = size.x;
        }
    }
}
void DisplayChannelsGroup::loadChildren(const boost::asio::any_io_executor& executor)
{
    if (children.empty() && group)
    {
        group->IterateChannels(
            [this, &executor](auto& channel)
            {
                children.emplace_back(
                    std::make_unique<DisplayChannel>(channel, this, executor));
                children.back().get()->indexInParent = children.size() - 1;
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
    // loadChildren();
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
        if (group && !group->AreChannelsLoaded())
        {
            ImGui::Text("Loading...");
        }
        else
        {
            isOpen = true;
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
DisplayNode::DisplayNode() : DisplayNode{ nullptr }
{
}
DisplayNode::DisplayNode(DisplayChannelsGroup* parent)
: DisplayNode{ "", parent }
{
}
DisplayNode::DisplayNode(const std::string& name, DisplayChannelsGroup* parent)
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
    isOpen = true;
}

DisplayChannel::DisplayChannel(ChannelPtr channel,
                               DisplayChannelsGroup* parent,
                               const boost::asio::any_io_executor& executor)
: DisplayNode{ channel->GetName(), parent }
, channel{ channel }
, displayLogoSize{ ImVec2{ (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.x * 2.f,
                           (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.y * 2.f } }
{
    decodeLogoImage(executor);
}
void DisplayChannel::decodeLogoImage(const boost::asio::any_io_executor& executor)
{
    if (!channel->GetLogo().empty())
    {
        boost::asio::post(executor,
                          [this]()
                          {
                              int width = 0;
                              int height = 0;
                              int channels = 0;
                              auto imageData = stbi_load_from_memory(
                                  reinterpret_cast<const stbi_uc*>(
                                      this->channel->GetLogo().data()),
                                  this->channel->GetLogo().size(), &width,
                                  &height, &channels, STBI_rgb_alpha);

                              float ratio = (float)width / (float)height;
                              ImVec2 size = displayLogoSize;
                              float area = size.x * size.y;
                              size.x = std::sqrt(ratio * area);
                              size.y = area / size.x;
                              displayLogoSize = size;

                              auto resizedImageData = stbir_resize_uint8_srgb(
                                  imageData, width, height, width * channels,
                                  nullptr, size.x, size.y, size.x * channels,
                                  (stbir_pixel_layout)channels);

                              stbi_image_free(imageData);
                              logoWidth = size.x;
                              logoHeight = size.y;
                              logoData = resizedImageData;
                              logoChannels = channels;
                          });
    }
}
void DisplayChannel::downloadLogoImage(const boost::asio::any_io_executor& executor)
{
    // TODO: implement this, one sunny day
    if (!channel->GetLogo().empty() || channel->GetLogoUri().empty())
        return;
}
DisplayChannel::~DisplayChannel()
{
    if (channelLogoTexture)
    {
        glDeleteTextures(1, &channelLogoTexture);
        channelLogoTexture = 0;
    }
    if (logoData)
    {
        stbi_image_free(logoData);
    }
}