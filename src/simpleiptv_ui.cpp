#include "simpleiptv_ui.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>


SimpleIPTVUI::SimpleIPTVUI(boost::asio::any_io_executor ui_executor,
                       WorkersProvider* workersProvider,
                       SimpleIPTVVulkan* vulkanInstance) :
 workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(
      ui_executor, this->workersProvider, vulkanInstance) }
, playerBarWindow{ PlayerBarWindow::Create(
      ui_executor, this->workersProvider, vulkanInstance) }
, epgListingWindow{ EpgListingWindow::Create(ui_executor, this->workersProvider) }
{
    using namespace std::placeholders;
    channelsWindow->AddChannelActivatedListener(
        std::bind(&SimpleIPTVUI::channelActivated, this, _1));
    playerBarWindow->AddNextChannelListener(
        [this]() { channelsWindow->ActivateNextChannel(); });
    playerBarWindow->AddPreviousChannelListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    
    playerBarWindow->AddEpgListingButtonChangedListener(
        [this](bool pressed) { epgListingWindow->SetClosed(!pressed); });
    epgListingWindow->AddChannelActivatedListener(
        [this](ChannelsGroupPtr group, ChannelPtr channel)
        { channelsWindow->ActivateChannelOfGroup(group, channel); });
    channelsWindow->AddScreenshotSettingsChangedListener(
        [this]() { screenshotSettingsChangedSignal(); });
    channelsWindow->AddTakeScreenshotListener([this]() { screenshotSignal(); });

    channelsWindow->GetPlayerSignal().connect(
        [this]() -> MpvPlayer*
        {
            auto result = getPlayerSignal();
            return result.value_or(nullptr);
        });
}

void SimpleIPTVUI::SetFullscreen(bool fullscreen)
{
    this->fullscreen = fullscreen;
    // Wake the main loop so it applies the window change immediately.
    glfwPostEmptyEvent();
}

ImRect SimpleIPTVUI::RenderDesktop()
{
    // In fullscreen the main loop owns the cursor (auto-hide); stop the ImGui
    // backend from re-asserting a visible cursor every frame. Must be set
    // before ImGui_ImplGlfw_NewFrame — that is where the backend applies it.
    if (fullscreen)
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
    else
    {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#ifdef STV_DEBUG
    if (showDemoWindow && !fullscreen)
        ImGui::ShowDemoWindow(&showDemoWindow);
#endif

    auto desktopRect = showDesktop();

    ImGui::Render();
    return desktopRect;
}

ImRect SimpleIPTVUI::showDesktop()
{
    if (ImGui::IsKeyPressed(ImGuiKey_Q) && ImGui::GetIO().KeyCtrl)
    {
        quit = true;
    }
    auto mainViewport = ImGui::GetMainViewport();

    ImRect desktopRect;
    if (fullscreen)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            SetFullscreen(false);
        }

        // No windows are drawn; the video gets the whole viewport.
        desktopRect.Min = mainViewport->WorkPos;
        desktopRect.Max = { mainViewport->WorkPos.x + mainViewport->WorkSize.x,
                            mainViewport->WorkPos.y + mainViewport->WorkSize.y };
    }
    else
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::GetIO().KeyCtrl && !ImGui::IsAnyItemHovered() &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        {
            playerBarWindow->SetChannelListPressed(
                !playerBarWindow->IsChannelListPressed());
        }

        ImVec2 playerBarSize = playerBarWindow->ShowWindow();
        if (playerBarWindow->IsChannelListPressed())
        {
            desktopRect.Min.x = channelsWindow->ShowWindow(playerBarSize.y).x;
            desktopRect.Min.y = 0;
            desktopRect.Max.x = playerBarSize.x;
            desktopRect.Max.y = mainViewport->WorkSize.y - playerBarSize.y;
        }
        else
        {
            desktopRect.Min = { 0.f, 0.f };
            desktopRect.Max = { playerBarSize.x,
                                mainViewport->WorkSize.y - playerBarSize.y };
        }
        if (!epgListingWindow->IsClosed())
        {
            epgListingWindow->ShowWindow();
        }
        else
        {
            playerBarWindow->SetEpgListingPressed(false);
        }

        // WantTextInput (rather than the hover gate below) so typing 'f' in
        // the channel filter or EPG search doesn't toggle fullscreen.
        if (playerBarWindow->GetCurrentPlayerState() == PlayerState::PLAYING &&
            !ImGui::GetIO().WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            SetFullscreen(true);
        }
    }

    if (!ImGui::IsAnyItemHovered() &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_M))
        {
            volumeToggleMuteSignal();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S))
        {
            screenshotSignal();
        }
        if (ImGui::GetIO().MouseWheel > 0)
        {
            volumeIncreaseSignal();
        }
        if (ImGui::GetIO().MouseWheel < 0)
        {
            volumeDecreaseSignal();
        }
    }
    return desktopRect;
}

void SimpleIPTVUI::channelActivated(ChannelPtr channel)
{
    spdlog::debug("{} activated", channel->GetName());
    playerBarWindow->SetCurrentChannel(channel);
    channelActivatedSignal(channel);
}
