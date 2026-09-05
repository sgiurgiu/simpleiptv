#include "channels_window.h"

#include <boost/asio/post.hpp>
#include <boost/url.hpp>
#include <chrono>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include "aboutwindow.h"
#include "display_tree_nodes/display_channel.h"
#include "epg/xmltv_epg_importer.h"
#include "fonts/IconsFontAwesome4.h"
#include "servers/server.h"

namespace
{
// constexpr float MAX_TIMEOUT = 5.f;
constexpr float INITIAL_BG_ALPHA = 0.6f;

std::shared_ptr<DisplayChannel> asDisplayChannel(DisplayNode* node)
{
    if (!dynamic_cast<DisplayChannel*>(node))
    {
        return nullptr;
    }
    return node->shared_from_base<DisplayChannel>();
}

/**
 * Wrap-around helper: opens `node` and returns its first (or last) child if
 * that child is a channel.
 */
std::shared_ptr<DisplayChannel>
openEdgeChannel(DisplayNode* node,
                WorkersProvider* workersProvider,
                SimpleIPTVVulkan* vulkanInstance,
                const boost::asio::any_io_executor& ui_executor,
                bool fromFront)
{
    node->loadChildren(workersProvider, vulkanInstance, ui_executor);
    if (node->children.empty())
    {
        return nullptr;
    }
    node->isOpen = true;
    return asDisplayChannel(fromFront ? node->children.front().get()
                                      : node->children.back().get());
}
} // namespace

std::shared_ptr<ChannelsWindow>
ChannelsWindow::Create(const boost::asio::any_io_executor& executor,
                       WorkersProvider* workersProvider,
                       SimpleIPTVVulkan* vulkanInstance)
{
    auto window = std::make_shared<ChannelsWindow>(
        Key{}, executor, workersProvider, vulkanInstance);
    window->initialize();
    return window;
}

ChannelsWindow::ChannelsWindow(Key,
                               const boost::asio::any_io_executor& ui_executor,
                               WorkersProvider* workersProvider,
                               SimpleIPTVVulkan* vulkanInstance)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, vulkanInstance{ vulkanInstance }
, bgAlpha{ INITIAL_BG_ALPHA }
, rootNode{ DisplayRootChannelsGroup::Create(workersProvider, ui_executor) }
, httpProxyDialog{ HTTPProxyDialog::Create(ui_executor, workersProvider) }
, screenshotDialog{ ScreenshotDialog::Create(workersProvider) }
, serverDialog{ ServerDialog::Create(ui_executor, workersProvider) }
, channelDialog{ ChannelDialog::Create(ui_executor, workersProvider) }
, aboutWindow{ vulkanInstance }
{
    width = workersProvider->GetSettingsRepository()->GetChannelsWindowWidth(300);
}

void ChannelsWindow::initialize()
{
    screenshotDialog->AddScreenshotSettingsChangedListener(
        [weak = weak_from_this()]()
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->screenshotSettingsChangedSignal();
        });

    rootNode->activatedChannelSignal.connect(
        [weak = weak_from_this()](std::shared_ptr<DisplayChannel> channel)
        {
            auto self = weak.lock();
            if (!self || !channel)
                return;
            self->setActivatedChannel(std::move(channel));
        });
    serverDialog->AddServersChangedListener(
        [weak = weak_from_this()]()
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->loadSavedServers();
        });
    channelDialog->AddChannelsChangedListener(
        [weak = weak_from_this()]()
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->loadLocalChannels();
        });

    loadLocalChannels();
    loadSavedServers();
}

void ChannelsWindow::setActivatedChannel(std::shared_ptr<DisplayChannel> channel)
{
    if (auto previous = activatedChannel.lock())
    {
        previous->isActivated = false;
    }
    activatedChannel = channel;
    channel->isActivated = true;
    channelActivatedSignal(channel->channel);
}

void ChannelsWindow::ActivateNextChannel()
{
    if (!rootNode || rootNode->children.empty())
    {
        return;
    }
    std::shared_ptr<DisplayChannel> channel;
    if (auto current = activatedChannel.lock())
    {
        auto next = current->getNextNode(workersProvider, vulkanInstance,
                                         ui_executor);
        current->isActivated = false;
        if (next)
        {
            while (next && !channel)
            {
                channel = asDisplayChannel(next);
                if (!channel)
                {
                    next = next->getNextNode(workersProvider, vulkanInstance,
                                             ui_executor);
                }
            }
            // Ran off the end without finding a channel: stay where we are.
            if (!channel)
            {
                channel = std::move(current);
            }
        }
        else
        {
            channel =
                openEdgeChannel(rootNode->children.front().get(),
                                workersProvider, vulkanInstance, ui_executor,
                                true);
        }
    }
    else
    {
        channel = openEdgeChannel(rootNode->children.front().get(),
                                  workersProvider, vulkanInstance, ui_executor,
                                  true);
    }

    if (channel)
    {
        setActivatedChannel(std::move(channel));
    }
}
void ChannelsWindow::ActivatePreviousChannel()
{
    if (!rootNode || rootNode->children.empty())
    {
        return;
    }
    std::shared_ptr<DisplayChannel> channel;
    if (auto current = activatedChannel.lock())
    {
        auto previous = current->getPreviousNode(workersProvider, vulkanInstance,
                                                 ui_executor);
        current->isActivated = false;
        if (previous)
        {
            while (previous && !channel)
            {
                channel = asDisplayChannel(previous);
                if (!channel)
                {
                    previous = previous->getPreviousNode(
                        workersProvider, vulkanInstance, ui_executor);
                }
            }
            if (!channel)
            {
                channel = std::move(current);
            }
        }
        else
        {
            channel =
                openEdgeChannel(rootNode->children.back().get(),
                                workersProvider, vulkanInstance, ui_executor,
                                false);
        }
    }
    else
    {
        channel = openEdgeChannel(rootNode->children.back().get(),
                                  workersProvider, vulkanInstance, ui_executor,
                                  false);
    }

    if (channel)
    {
        setActivatedChannel(std::move(channel));
    }
}

ImVec2 ChannelsWindow::ShowWindow(float playerBarHeight)
{
    /*if (!forceDisplay)
    {
        if (ImGui::GetCurrentContext()->MouseStationaryTimer >= MAX_TIMEOUT)
        {
            bgAlpha = INITIAL_BG_ALPHA;
            return;
        }

        if (ImGui::GetCurrentContext()->MouseStationaryTimer > (MAX_TIMEOUT
    / 3.f))
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
    }*/

    auto mainViewport = ImGui::GetMainViewport();
    ImVec2 size;
    size.x = width.value_or(mainViewport->WorkSize.x * 0.2f);
    size.y = mainViewport->WorkSize.y - ImGui::GetStyle().WindowBorderSize -
             playerBarHeight;

    ImGui::SetNextWindowPos(
        ImVec2(mainViewport->WorkPos.x, mainViewport->WorkPos.y));
    ImGui::SetNextWindowSize(size, ImGuiCond_None);
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    if (!ImGui::Begin("Channels", nullptr,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return { 0, 0 };
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

    httpProxyDialog->ShowDialog();
    screenshotDialog->ShowDialog();
    serverDialog->ShowDialog();
    channelDialog->ShowDialog();
    colorspaceDialog.ShowColorspaceDialog();
    aboutWindow.ShowAboutWindow();
    if (width != (int)std::floor(ImGui::GetWindowSize().x))
    {
        width = std::floor(ImGui::GetWindowSize().x);
        workersProvider->GetSettingsRepository()->SetChannelsWindowWidth(
            width.value());
    }

    ImGui::End();

    return size;
}

void ChannelsWindow::showLocalChannelsTab()
{
    ImGui::InputTextWithHint("##filterChannels", "Filter", &channelsFilter);

    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_ERASER)))
    {
        channelsFilter.clear();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::BeginChild("##localChannelsTab", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    rootNode->render(localSelectedNodes, channelsFilter);
    ImGui::EndChild();
}
void ChannelsWindow::showRemoteChannelsTab()
{
    ImGui::BeginChild("##remoteChannelsTab", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& s : servers)
    {
        s->render(remoteSelectedNodes, "");
    }
    ImGui::EndChild();
}

void ChannelsWindow::loadLocalChannels()
{
    auto start = std::chrono::high_resolution_clock::now();
    spdlog::debug("starting to load channels");
    workersProvider->GetChannelsRepository()->LoadChannelsAndGroups(
        [weak = weak_from_this(), start](RootChannelsGroupPtr root)
        {
            auto self = weak.lock();
            if (!self)
                return;

            self->rootNode->setRoot(root, self->workersProvider,
                                    self->vulkanInstance, self->ui_executor);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = end - start;
            spdlog::debug(
                "done loading channels. duration: {} ms",
                std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                    .count());
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
                serverDialog->SetShowAddServerDialog();
            }
            if (ImGui::MenuItem("Add Single Channel"))
            {
                channelDialog->SetShowAddChannelDialog();
            }

            ImGui::SetNextItemShortcut(ImGuiKey_S, ImGuiInputFlags_RouteGlobal |
                                                       ImGuiInputFlags_Tooltip);
            if (ImGui::MenuItem("Take Screenshot", "S"))
            {
                takeScreenshotSignal();
            }
            ImGui::Separator();
            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_Q,
                                       ImGuiInputFlags_RouteGlobal |
                                           ImGuiInputFlags_Tooltip);
            ImGui::MenuItem("Quit", "Ctrl+Q", &quit);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("HTTP Proxy"))
            {
                httpProxyDialog->SetShowHTTPProxyDialog(true);
            }
            if (ImGui::MenuItem("Screenshot"))
            {
                screenshotDialog->SetShowScreenshotDialog(true);
            }

            if (ImGui::BeginMenu("Colorspace"))
            {
                auto player = getPlayerSignal();
                if (!player)
                {
                    ImGui::EndMenu();
                    return;
                }
                colorspaceDialog.ShowColorspaceMenus(player.value());
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                aboutWindow.SetWindowShowing(true);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

bool ChannelsWindow::ActivateChannelOfGroup(ChannelsGroupPtr group,
                                            ChannelPtr channel)
{
    return rootNode->ActivateChannelOfGroup(group, channel);
}

void ChannelsWindow::loadSavedServers()
{
    workersProvider->GetServersRepository()->LoadServers(
        [weak = weak_from_this()](std::vector<ServerPtr> servers)
        {
            auto self = weak.lock();
            if (!self)
                return;

            self->servers.clear();
            for (const auto& s : servers)
            {
                auto server = DisplayServer::Create(self->workersProvider,
                                                    self->ui_executor, s);
                server->editServerSignal.connect(
                    [weak = self->weak_from_this()](ServerPtr server)
                    {
                        auto self = weak.lock();
                        if (!self)
                            return;
                        self->serverDialog->SetShowEditServerDialog(server);
                    });
                server->removeServerSignal.connect(
                    [weak = self->weak_from_this()](ServerPtr server)
                    {
                        auto self = weak.lock();
                        if (!self)
                            return;
                        self->serverDialog->SetShowRemoveServerDialog(server);
                    });
                server->activatedChannelSignal.connect(
                    [weak = self->weak_from_this()](
                        std::shared_ptr<DisplayChannel> channel)
                    {
                        auto self = weak.lock();
                        if (!self || !channel)
                            return;
                        self->setActivatedChannel(std::move(channel));
                    });
                server->reloadLocalChannelsSignal.connect(
                    [weak = self->weak_from_this()]()
                    {
                        auto self = weak.lock();
                        if (!self)
                            return;
                        self->loadLocalChannels();
                    });

                self->servers.push_back(server);
                self->loadServerXmlTv(s);
            }
        },
        ui_executor);
}

void ChannelsWindow::loadServerXmlTv(ServerPtr server)
{
    // Skip servers whose guide we refreshed recently, so we don't re-download
    // ~100 MB on every launch.
    auto updatedAt = server->GetXmlTvUpdatedAt();
    if (updatedAt && std::chrono::system_clock::now() - *updatedAt <
                         std::chrono::hours{ 12 })
    {
        spdlog::info("XMLTV for server {} is fresh; skipping refresh",
                     server->GetId());
        return;
    }

    auto importer = XmlTvEpgImporter::Create(workersProvider);
    importer->Import(
        server,
        [id = server->GetId()](std::error_code ec)
        {
            if (ec)
                spdlog::error("XMLTV import failed for server {}: {}", id,
                              ec.message());
            else
                spdlog::debug("XMLTV import complete for server {}", id);
        },
        ui_executor);
}
