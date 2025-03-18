#include "channels_window.h"

#include <boost/asio/post.hpp>
#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>
#include <unordered_set>

#include "fonts/IconsFontAwesome4.h"

namespace
{
// constexpr float MAX_TIMEOUT = 5.f;
constexpr float INITIAL_BG_ALPHA = 0.6f;
static std::unordered_set<DisplayNode*> localSelectedNodes;
static std::unordered_set<DisplayNode*> remoteSelectedNodes;
} // namespace

std::shared_ptr<ChannelsWindow>
ChannelsWindow::Create(const boost::asio::any_io_executor& executor,
                       WorkersProvider* workersProvider)
{
    auto window =
        std::make_shared<ChannelsWindow>(Key{}, executor, workersProvider);
    window->initialize();
    return window;
}

ChannelsWindow::ChannelsWindow(Key,
                               const boost::asio::any_io_executor& ui_executor,
                               WorkersProvider* workersProvider)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, bgAlpha{ INITIAL_BG_ALPHA }
, rootNode{ DisplayRootChannelsGroup::Create() }
, httpProxyDialog{ HTTPProxyDialog::Create(ui_executor, workersProvider) }
{
}

void ChannelsWindow::initialize()
{
    rootNode->activatedChannelSignal.connect(
        [weak = weak_from_this()](DisplayChannel* channel)
        {
            auto self = weak.lock();
            if (!self)
                return;
            if (self->activatedChannel)
            {
                self->activatedChannel->isActivated = false;
            }
            self->activatedChannel = channel;
            self->activatedChannel->isActivated = true;
            self->channelActivatedSignal(channel->channel);
        });
    loadLocalChannels();
    loadSavedServers();
}

void ChannelsWindow::ActivateNextChannel()
{
    if (activatedChannel)
    {
        auto next = activatedChannel->getNextNode(workersProvider, ui_executor);
        activatedChannel->isActivated = false;
        if (next)
        {
            DisplayChannel* channel = nullptr;
            while (next && channel == nullptr)
            {
                channel = dynamic_cast<DisplayChannel*>(next);
                if (channel)
                {
                    activatedChannel = channel;
                }
                else
                {
                    next = next->getNextNode(workersProvider, ui_executor);
                }
            }
        }
        else
        {
            rootNode->children.begin()->get()->loadChildren(workersProvider,
                                                            ui_executor);
            if (!rootNode->children.begin()->get()->children.empty())
            {
                rootNode->children.begin()->get()->isOpen = true;
                activatedChannel = dynamic_cast<DisplayChannel*>(
                    rootNode->children.begin()->get()->children.begin()->get());
            }
        }
    }
    else
    {
        rootNode->children.begin()->get()->loadChildren(workersProvider,
                                                        ui_executor);
        if (!rootNode->children.begin()->get()->children.empty())
        {
            rootNode->children.begin()->get()->isOpen = true;
            activatedChannel = dynamic_cast<DisplayChannel*>(
                rootNode->children.begin()->get()->children.begin()->get());
        }
    }

    if (activatedChannel)
    {
        activatedChannel->isActivated = true;
        channelActivatedSignal(activatedChannel->channel);
    }
}
void ChannelsWindow::ActivatePreviousChannel()
{
    if (activatedChannel)
    {
        auto next =
            activatedChannel->getPreviousNode(workersProvider, ui_executor);
        activatedChannel->isActivated = false;
        if (next)
        {
            DisplayChannel* channel = nullptr;
            while (next && channel == nullptr)
            {
                channel = dynamic_cast<DisplayChannel*>(next);
                if (channel)
                {
                    activatedChannel = channel;
                }
                else
                {
                    next = next->getPreviousNode(workersProvider, ui_executor);
                }
            }
        }
        else
        {
            rootNode->children.rbegin()->get()->loadChildren(workersProvider,
                                                             ui_executor);
            if (!rootNode->children.rbegin()->get()->children.empty())
            {
                rootNode->children.rbegin()->get()->isOpen = true;
                activatedChannel = dynamic_cast<DisplayChannel*>(
                    rootNode->children.rbegin()->get()->children.rbegin()->get());
            }
        }
    }
    else
    {
        rootNode->children.rbegin()->get()->loadChildren(workersProvider,
                                                         ui_executor);
        if (!rootNode->children.rbegin()->get()->children.empty())
        {
            rootNode->children.rbegin()->get()->isOpen = true;
            activatedChannel = dynamic_cast<DisplayChannel*>(
                rootNode->children.rbegin()->get()->children.rbegin()->get());
        }
    }

    if (activatedChannel)
    {
        activatedChannel->isActivated = true;
        channelActivatedSignal(activatedChannel->channel);
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
    size.x = std::fmin(mainViewport->WorkSize.x * 0.2f, 300.f);
    size.y = mainViewport->WorkSize.y - ImGui::GetStyle().WindowBorderSize -
             playerBarHeight;

    ImGui::SetNextWindowPos(
        ImVec2(mainViewport->WorkPos.x, mainViewport->WorkPos.y));
    ImGui::SetNextWindowSize(size, ImGuiCond_None);
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    if (!ImGui::Begin("Channels", nullptr,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_MenuBar))
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
                                    self->ui_executor);
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
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void ChannelsWindow::ActivateChannelOfGroup(ChannelsGroupPtr group,
                                            ChannelPtr channel)
{
    rootNode->ActivateChannelOfGroup(group, channel);
}

void ChannelsWindow::loadSavedServers()
{
    workersProvider->GetServersRepository()->LoadServers(
        [weak = weak_from_this()](std::vector<ServerPtr> servers)
        {
            auto self = weak.lock();
            if (!self)
                return;

            for (const auto& s : servers)
            {
                auto server = DisplayServer::Create(self->workersProvider,
                                                    self->ui_executor, s);
                server->activatedChannelSignal.connect(
                    [weak = self->weak_from_this()](DisplayChannel* channel)
                    {
                        auto self = weak.lock();
                        if (!self)
                            return;
                        if (self->activatedChannel)
                        {
                            self->activatedChannel->isActivated = false;
                        }
                        self->activatedChannel = channel;
                        self->activatedChannel->isActivated = true;
                        self->channelActivatedSignal(channel->channel);
                    });

                self->servers.push_back(server);
            }
        },
        ui_executor);
}