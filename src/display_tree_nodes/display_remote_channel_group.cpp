#include "display_remote_channel_group.h"

#include <boost/asio/post.hpp>
#include <boost/url.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>

#include "../fonts/IconsFontAwesome4.h"
#include "../workers_provider.h"
#include "common.h"
#include "display_channel.h"
#include "display_node.h"
#include "display_server_node.h"
#include "imgui_internal.h"

DisplayRemoteChannelsGroup::DisplayRemoteChannelsGroup(
    DisplayNodeKey key,
    const std::string& name,
    const std::string& url,
    WorkersProvider* workersProvider,
    const boost::asio::any_io_executor& ui_executor,
    DisplayServerCategory* parent)
: DisplayChannelsGroup{ key, name, workersProvider, ui_executor, parent }
, serverCategory{ parent }
, url{ url }
, channelsFilterLabel{ fmt::format("##filterRemoteChannels{}", name) }
, eraserFilterLabel{ fmt::format("{}##eraserRemoteChannelsFilter{}",
                                 reinterpret_cast<const char*>(ICON_FA_ERASER),
                                 name) }
{
}

bool DisplayRemoteChannelsGroup::shouldRender(const std::string& filter) const
{
    if (filter.empty())
        return true;

    auto it = std::search(name.cbegin(), name.cend(), filter.cbegin(),
                          filter.cend(), [](char c1, char c2)
                          { return std::tolower(c1) == std::tolower(c2); });

    return it != name.cend();
}

void DisplayRemoteChannelsGroup::render(SelectionSet& selectedNodes,
                                        const std::string& filter)
{
    if (!shouldRender(filter))
    {
        return;
    }
    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selected)
    {
        tree_node_flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::PushID(this);

    ImGui::SetNextItemOpen(isOpen);
    if (ImGui::TreeNodeEx(name.c_str(), tree_node_flags))
    {
        showPopup(name.c_str());
        isOpen = true;
        if (error)
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored({ 1.f, 0.f, 0.f, 1.f }, "Error loading: %s",
                               error->c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Retry"))
            {
                error.reset();
                areChildrenLoaded = false;
                areChildrenLoading = true;
                loadRemoteChildren();
            }
        }
        else if (!areChildrenLoaded)
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
            ImGui::InputTextWithHint(channelsFilterLabel.c_str(), "Filter",
                                     &channelsFilter);
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
            if (ImGui::Button(eraserFilterLabel.c_str()))
            {
                channelsFilter.clear();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            for (auto& c : children)
            {
                c->render(selectedNodes, channelsFilter);
            }
        }
        ImGui::TreePop();
    }
    else if (ImGui::IsItemToggledOpen())
    {
        isOpen = false;
        clearSelectedChildren(this, selectedNodes);
    }
    showPopup(name.c_str());
    ImGui::PopID();
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
                self->error = ec.message();
                self->areChildrenLoading = false;
                return;
            }

            if (self->areChildrenLoaded)
            {
                return;
            }
            DisplayNode* node = selfNode.get();
            while (node && node->parent &&
                   node->parent->getType() != DisplayNodeType::SERVER)
            {
                node = node->parent;
            }
            if (!node)
            {
                return;
            }
            self->children.clear();
            DisplayServer* server = static_cast<DisplayServer*>(node);
            int server_id = server->getUnderlyingID();

            try
            {
                nlohmann::json json = nlohmann::json::parse(body);
                for (const auto& ch : json)
                {
                    auto streamId = ch["stream_id"].get<int>();
                    auto streamType = ch["stream_type"].get<std::string>();
                    auto name = ch["name"].get<std::string>();
                    auto icon = ch["stream_icon"].get<std::string>();
                    auto epgStreamId = ch["epg_channel_id"].get<std::string>();

                    boost::url channelUrl =
                        boost::urls::parse_uri(self->url).value();
                    boost::url epgUrl = channelUrl;

                    auto channelParams = channelUrl.params();
                    auto username = (*channelParams.find("username"))->value;
                    auto password = (*channelParams.find("password"))->value;
                    channelParams.clear();

                    channelUrl.set_path(fmt::format("/{}/{}/{}/{}.ts",
                                                    streamType, username,
                                                    password, streamId));
                    auto epgParams = epgUrl.params();
                    epgParams.replace(epgParams.find("action"),
                                      { "action", "get_simple_data_table" });
                    epgParams.set("stream_id", std::to_string(streamId));

                    auto channel = std::make_shared<Channel>(
                        -1, name, channelUrl.buffer(), icon, "",
                        epgUrl.buffer(), epgStreamId, server_id, false, -1);

                    auto displayChannel = DisplayChannel::Create(
                        channel, self->workersProvider, nullptr,
                        self->ui_executor, self.get());
                    displayChannel->reloadLocalChannelsSignal.connect(
                        std::ref(self->reloadLocalChannelsSignal));
                    self->children.push_back(std::move(displayChannel));
                }
            }
            catch (const std::exception& ex)
            {
                self->error = ex.what();
                self->areChildrenLoading = false;
                return;
            }
            self->error.reset();
            self->areChildrenLoaded = true;
            self->areChildrenLoading = false;
            for (const auto& f : self->saveGroupCallbacks)
            {
                boost::asio::post(self->ui_executor, f);
            }
            self->saveGroupCallbacks.clear();
        },
        false);
}

void DisplayRemoteChannelsGroup::showPopup(const char* id)
{
    if (ImGui::BeginPopupContextItem(id))
    {
        if (ImGui::Selectable("Save/Update Group"))
        {
            // Now, this may seem complicated, but it isn't really
            // if children are loaded, then we're good
            // if they are not, but are loading, it means that we're now in the
            // process of retrieving data from the server and next frame we may
            // see the callback in loadRemoteChildren() get called. if neither
            // are true, then we can just call loadRemoteChildren() safely.
            if (areChildrenLoaded)
            {
                // save update children
                saveGroupLocally();
            }
            else
            {
                saveGroupCallbacks.emplace_back(std::bind(
                    &DisplayRemoteChannelsGroup::saveGroupLocally, this));
                if (areChildrenLoading)
                {
                    // we just need to wait for the currently executing
                    // loadRemoteChildren() to complete
                }
                else
                {
                    areChildrenLoading = true;
                    loadRemoteChildren();
                }
            }
        }
        if (!areChildrenLoading && ImGui::Selectable("Refresh"))
        {
            areChildrenLoaded = false;
            areChildrenLoading = true;
            loadRemoteChildren();
        }
        ImGui::EndPopup();
    }
}

void DisplayRemoteChannelsGroup::saveGroupLocally()
{
    ChannelsGroupPtr group =
        std::make_shared<ChannelsGroup>(-1, name, std::nullopt);
    for (const auto& child : children)
    {
        std::shared_ptr<DisplayChannel> channel =
            std::static_pointer_cast<DisplayChannel>(child);
        group->AddChannel(channel->channel);
    }
    workersProvider->GetChannelsRepository()->UpsertGroup(
        group, [self = shared_from_this()](auto)
        { self->reloadLocalChannelsSignal(); }, ui_executor);
}
