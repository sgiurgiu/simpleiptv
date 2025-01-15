#include <GL/glew.h>

#include "display_tree_node.h"

#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/url.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include "fonts/IconsFontAwesome4.h"
#include "workers_provider.h"

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

DisplayNode* DisplayNode::getNextNode(WorkersProvider* workersProvider,
                                      const boost::asio::any_io_executor& ui_executor)
{
    DisplayNode* curr_node = this;
    curr_node->loadChildren(workersProvider, ui_executor);
    if (!curr_node->children.empty())
    {
        isOpen = true;
        return curr_node->children.begin()->get();
    }
    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(workersProvider, ui_executor);
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

DisplayNode*
DisplayNode::getPreviousNode(WorkersProvider* workersProvider,
                             const boost::asio::any_io_executor& ui_executor)
{
    DisplayNode* curr_node = this;

    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(workersProvider, ui_executor);
        if (curr_node->indexInParent > 0)
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent - 1).get();
            node->loadChildren(workersProvider, ui_executor);
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
                                       WorkersProvider* workersProvider,
                                       const boost::asio::any_io_executor& ui_executor)
{
    children.clear();
    this->root = root;
    this->group = root;
    favouritesGroup = DisplayFavouritesChannelsGroup::Create(this);
    root->IterateFavouriteChannels(
        [this, workersProvider, ui_executor](ChannelPtr channel)
        {
            auto dchannel =
                DisplayChannel::Create(channel, favouritesGroup.get());
            dchannel->indexInParent = favouritesGroup->children.size();
            dchannel->loadLogo(workersProvider, ui_executor);
            favouritesGroup->children.push_back(std::move(dchannel));
        });
    favouritesGroup->indexInParent = 0;
    children.push_back(std::move(favouritesGroup));
    loadChildren(workersProvider, ui_executor);
}

void DisplayRootChannelsGroup::loadChildren(
    WorkersProvider* workersProvider,
    const boost::asio::any_io_executor& ui_executor)
{
    if (!root || !root->AreGroupsLoaded())
    {
        return;
    }

    if (children.size() < 2 && root->AreGroupsLoaded())
    {
        root->IterateGroups(
            [this, workersProvider, ui_executor](ChannelsGroupPtr group)
            {
                children.emplace_back(DisplayChannelsGroup::Create(group, this));
                children.back().get()->indexInParent = children.size() - 1;
                children.back().get()->loadChildren(workersProvider, ui_executor);
            });
    }
}

bool DisplayChannel::shouldRender(const std::string& filter) const
{
    if (filter.empty())
        return true;
    auto it = std::search(name.cbegin(), name.cend(), filter.cbegin(),
                          filter.cend(), [](char c1, char c2)
                          { return std::tolower(c1) == std::tolower(c2); });
    return it != name.cend();
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
    if (shouldScrollToChannel)
    {
        ImGui::ScrollToItem(ImGuiScrollFlags_None);
        shouldScrollToChannel = false;
    }
    ImGui::PopID();
    if (isActivated && !isSelected)
    {
        ImGui::PopStyleColor(1);
    }
    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        activate();
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
    scrollY = ImGui::GetScrollY();
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
void DisplayChannelsGroup::loadChildren(WorkersProvider* workersProvider,
                                        const boost::asio::any_io_executor& ui_executor)
{
    if (children.empty() && group)
    {
        group->IterateChannels(
            [this, workersProvider, ui_executor](auto& channel)
            {
                auto dchannel = DisplayChannel::Create(channel, this);
                children.emplace_back(dchannel);
                dchannel->loadLogo(workersProvider, ui_executor);
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
DisplayNode::DisplayNode(DisplayNodeKey key) : DisplayNode{ key, nullptr }
{
}
DisplayNode::DisplayNode(DisplayNodeKey key, DisplayChannelsGroup* parent)
: DisplayNode{ key, "", parent }
{
}
DisplayNode::DisplayNode(DisplayNodeKey,
                         const std::string& name,
                         DisplayChannelsGroup* parent)
: parent{ parent }, name{ name }
{
    if (parent)
    {
        activatedChannelSignal.connect(parent->activatedChannelSignal);
    }
}
DisplayFavouritesChannelsGroup::DisplayFavouritesChannelsGroup(
    DisplayNodeKey key, DisplayRootChannelsGroup* parent)
: DisplayChannelsGroup{
    key, reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"), parent
}
{
    isOpen = true;
}

DisplayChannel::DisplayChannel(DisplayNodeKey key,
                               ChannelPtr channel,
                               DisplayChannelsGroup* parent)
: DisplayNode{ key, channel->GetName(), parent }
, channel{ channel }
, displayLogoSize{ ImVec2{ (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.x * 2.f,
                           (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.y * 2.f } }
{
}
void DisplayChannel::loadLogo(WorkersProvider* workersProvider,
                              const boost::asio::any_io_executor& ui_executor)
{
    if (!channel->IsLogoEmpty())
    {
        decodeLogoImage(workersProvider->GetNetworkExecutor());
    }
    else
    {
        downloadLogoImage(workersProvider, ui_executor);
    }
}
void DisplayChannel::decodeLogoImage(const boost::asio::any_io_executor& executor)
{
    if (!channel->IsLogoEmpty())
    {
        boost::asio::post(
            executor,
            [weak = this->weak_from_this()]()
            {
                auto selfNode = weak.lock();
                if (!selfNode)
                    return;
                auto self = std::static_pointer_cast<DisplayChannel>(selfNode);
                self->decodeLogoImage();
            });
    }
}
void DisplayChannel::decodeLogoImage()
{
    int width = 0;
    int height = 0;
    int channels = 0;
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(channel->GetLogoData()),
        channel->GetLogoSize(), &width, &height, &channels, STBI_rgb_alpha);

    float ratio = (float)width / (float)height;
    ImVec2 size = displayLogoSize;
    float area = size.x * size.y;
    size.x = std::sqrt(ratio * area);
    size.y = area / size.x;
    displayLogoSize = size;

    auto resizedImageData = stbir_resize_uint8_srgb(
        imageData, width, height, width * channels, nullptr, size.x, size.y,
        size.x * channels, (stbir_pixel_layout)channels);

    stbi_image_free(imageData);
    logoWidth = size.x;
    logoHeight = size.y;
    logoData = resizedImageData;
    logoChannels = channels;
}
void DisplayChannel::downloadLogoImage(WorkersProvider* workersProvider,
                                       const boost::asio::any_io_executor& ui_executor)
{
    if (!channel->IsLogoEmpty() || channel->GetLogoUri().empty())
        return;

    /**
     * We just need a thread (a different one than the UI thread and the network
     * threads) to get the callback into. We, therefore chose the DB thread as
     * it kinda seems like a waste to have a thread that will only be used once
     * in the lifetime of the application (when first downloading) the channels
     * logos. We can't use the network threads pool because those will be used
     * at maximum when we're downloading thousands of logos from the internet,
     * so the callback(s) will only be called very late in the process. We can't
     * use the UI thread because then we cannot play any channel while the logos
     * are downloading. It would make for a shitty first experience for the
     * user. So, we're just using the DB thread here as the callback receiver
     * (workersProvider->GetDBExecutor()).
     */
    workersProvider->GetNetworkResourceProvider()->GetResource(
        channel->GetLogoUri(), workersProvider->GetDBExecutor(),
        [weak = weak_from_this(), workersProvider,
         ui_executor](std::string logo, std::error_code ec)
        {
            auto selfNode = weak.lock();
            if (!selfNode)
                return;
            auto self = std::static_pointer_cast<DisplayChannel>(selfNode);
            if (ec)
            {
                spdlog::error("Cannot download '{}', failed with error: {}",
                              self->channel->GetLogoUri(), ec.message());
                return;
            }
            spdlog::debug("Downloaded logo for {}, from {}",
                          self->channel->GetName(), self->channel->GetLogoUri());
            // Logo Get<thing>/Set is protected by a mutex in Channel
            self->channel->SetLogo(logo);
            workersProvider->GetChannelsRepository()->UpdateChannelLogoSync(
                self->channel->GetId(), std::move(logo));
            self->decodeLogoImage();
        });
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
void DisplayRootChannelsGroup::ActivateChannelOfGroup(ChannelsGroupPtr group,
                                                      ChannelPtr channel)
{
    DisplayNodeType groupType = DisplayNodeType::GROUP;
    if (group->GetId() < 0 && channel->IsFavourite())
    {
        // we got a special group
        groupType = DisplayNodeType::FAVOURITES;
    }

    auto findGroup = [group, groupType](this auto const& findGroup,
                                        DisplayNode* node) -> DisplayNode*
    {
        DisplayNode* foundNode = nullptr;
        for (const auto& child : node->children)
        {
            if (child->getType() == groupType &&
                child->getUnderlyingID() == group->GetId())
            {
                foundNode = child.get();
                break;
            }
            else
            {
                foundNode = findGroup(child.get());
            }
        }
        return foundNode;
    };

    auto foundGroup = findGroup(this);
    if (foundGroup)
    {
        foundGroup->isOpen = true;
        auto channelIt =
            std::find_if(foundGroup->children.begin(), foundGroup->children.end(),
                         [id = channel->GetId()](const auto& c)
                         {
                             return id == c->getUnderlyingID() &&
                                    c->getType() == DisplayNodeType::CHANNEL;
                         });
        if (channelIt != foundGroup->children.cend())
        {
            DisplayChannel* channel =
                dynamic_cast<DisplayChannel*>(channelIt->get());
            if (channel)
            {
                channel->activate();
            }
        }
    }
}
void DisplayChannel::activate()
{
    activatedChannelSignal(this);
    isActivated = true;
    shouldScrollToChannel = true;
    spdlog::debug("activated {} - {}", name, channel->GetUri());
}

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

    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
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
}

DisplayServer::DisplayServer(DisplayNodeKey key,
                             WorkersProvider* workersProvider,
                             const boost::asio::any_io_executor& ui_executor,
                             ServerPtr server)
: DisplayChannelsGroup{ key,
                        reinterpret_cast<const char*>(ICON_FA_SERVER " ") +
                            server->GetHost(),
                        nullptr }
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

    children.push_back(DisplayServerCategory::Create(
        reinterpret_cast<const char*>(ICON_FA_TELEVISION " Live"), url.buffer(),
        workersProvider, ui_executor, this));
    params.replace(params.find("action"), { "action", "get_vod_categories" });
    children.push_back(DisplayServerCategory::Create(
        reinterpret_cast<const char*>(ICON_FA_VIDEO_CAMERA " VODs"),
        url.buffer(), workersProvider, ui_executor, this));
}
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
                    DisplayChannel::Create(channel, self.get()));
            }
        },
        false);
}