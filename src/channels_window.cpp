#include "channels_window.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <spdlog/spdlog.h>

namespace
{
constexpr float MAX_TIMEOUT = 10.f;
constexpr float INITIAL_BG_ALPHA = 0.5f;
} // namespace

ChannelsWindow::ChannelsWindow(const boost::asio::any_io_executor& ui_executor)
: ui_executor{ ui_executor }, remote_loading_pool{ 6 }, bgAlpha{ INITIAL_BG_ALPHA }
{
    loadLocalChannels();
}
ChannelsWindow::~ChannelsWindow()
{
    remote_loading_pool.stop();
    remote_loading_pool.join();
}

void ChannelsWindow::showWindow()
{
    if (ImGui::GetCurrentContext()->MouseStationaryTimer >= MAX_TIMEOUT)
    {
        bgAlpha = INITIAL_BG_ALPHA;
        return;
    }

    if (ImGui::GetCurrentContext()->MouseStationaryTimer > 3.f)
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
}
void ChannelsWindow::showRemoteChannelsTab()
{
}

void ChannelsWindow::loadLocalChannels()
{
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