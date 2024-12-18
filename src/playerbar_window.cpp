#include "playerbar_window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <spdlog/spdlog.h>

#include "fonts/IconsFontAwesome4.h"

PlayerBarWindow::PlayerBarWindow(const boost::asio::any_io_executor& ui_executor)
: ui_executor{ ui_executor }
{
}
ImVec2 PlayerBarWindow::ShowWindow()
{
    auto mainViewport = ImGui::GetMainViewport();

    ImVec2 size;
    size.x = mainViewport->WorkSize.x - ImGui::GetStyle().WindowBorderSize * 2.f;
    size.y = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.f +
             ImGui::GetStyle().WindowPadding.y * 2.f;

    ImGui::SetNextWindowPos(
        ImVec2(mainViewport->WorkPos.x, mainViewport->WorkSize.y - size.y));
    ImGui::SetNextWindowSize(size, ImGuiCond_None);
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    if (!ImGui::Begin("Player Bar", nullptr,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          // ImGuiWindowFlags_NoBackground |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoDecoration |
                          ImGuiWindowFlags_NoFocusOnAppearing |
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::End();
        return { 0, 0 };
    }

    ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
    // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);

    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STEP_BACKWARD));
    ImGui::SameLine();
    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_PLAY));
    ImGui::SameLine();
    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STOP));
    ImGui::SameLine();
    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STEP_FORWARD));
    ImGui::SameLine();
    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_VOLUME_UP));
    bool isVolumeHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);
    if (isVolumeHovered || isVolumeSliderHovered ||
        (std::chrono::steady_clock::now() - lastVolumeHoveredTime <=
         std::chrono::seconds{ 2 }))
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        ImGui::SliderInt("##vol_slider", &volume, 0, 150, "%d",
                         ImGuiSliderFlags_NoRoundToFormat);
        isVolumeSliderHovered = ImGui::IsItemHovered();
    }
    ImGui::PopStyleColor(2);

    if (isVolumeHovered || isVolumeSliderHovered)
    {
        lastVolumeHoveredTime = std::chrono::steady_clock::now();
    }

    if (currentChannel)
    {
        ImGui::SameLine();
        ImGui::Text("%s", currentChannel->GetName().c_str());
    }
    auto localPosition = ImGui::GetCursorPosX();
    auto availableSpace = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availableSpace - localPosition -
                    ImGui::GetStyle().FramePadding.x);

    if (channelListPressed)
    {
        auto color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);
    }

    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_LIST)))
    {
        channelListPressed = !channelListPressed;
    }

    ImGui::PopStyleColor(2);

    ImGui::End();
    return size;
}