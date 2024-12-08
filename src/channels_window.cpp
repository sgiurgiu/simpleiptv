#include "channels_window.h"

#include <boost/asio/post.hpp>
#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

namespace
{
constexpr float MAX_TIMEOUT = 10.f;
constexpr float INITIAL_BG_ALPHA = 0.5f;
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

void ChannelsWindow::showWindow()
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
    if (!root)
        return;

    root->IterateGroups(
        [](ChannelsGroupPtr group)
        {
            if (ImGui::TreeNode(group->GetName().c_str()))
            {
                group->IterateChannels(
                    [](ChannelPtr channel)
                    { ImGui::BulletText(channel->GetName().c_str()); });
                ImGui::TreePop();
            }
        });
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

            self->root = root;
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