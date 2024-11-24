#include "channels_window.h"

#include <imgui.h>

#include <spdlog/spdlog.h>

ChannelsWindow::ChannelsWindow(const boost::asio::any_io_executor& ui_executor)
: ui_executor{ ui_executor }
{
    loadChannels();
}

void ChannelsWindow::showWindow()
{
    auto mousePos = ImGui::GetIO().MousePos;

    if (mousePos.x < 0.f || mousePos.x > 100.f)
    {
        return;
    }
    auto mainViewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(
        ImVec2(mainViewport->WorkPos.x, mainViewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(mainViewport->WorkSize.x * 0.2f,
                                    mainViewport->WorkSize.y * 0.9f),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (!ImGui::Begin("Channels", nullptr, ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        return;
    }

    ImGui::End();
}

void ChannelsWindow::loadChannels()
{
}