#include "simpleiptv_ui.h"

#include <imgui_impl_glfw.h>
#include <imgui.h>
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

ImRect SimpleIPTVUI::RenderDesktop()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#ifdef STV_DEBUG
    if (showDemoWindow)
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

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ImGui::GetIO().KeyCtrl && !ImGui::IsAnyItemHovered() &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        playerBarWindow->SetChannelListPressed(
            !playerBarWindow->IsChannelListPressed());
    }
    auto mainViewport = ImGui::GetMainViewport();

    ImRect desktopRect;
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

/*Solution 3: Recreate swapchain asynchronously (Advanced)
This is more complex but gives the smoothest experience:
cvoid SimpleIPTV::setSize(int width, int height)
{
    this->width = width;
    this->height = height;

    // Post resize work to background thread
    resizeWorkQueue.post([this, width, height]() {
        // Create new swapchain on background thread
        auto newSwapchain = createSwapchain(width, height);

        // Swap on main thread
        uiContext.post([this, newSwapchain]() {
            destroyOldSwapchain();
            this->swapchain = newSwapchain;
        });
    });
}*/


void SimpleIPTVUI::channelActivated(ChannelPtr channel)
{
    spdlog::debug("{} activated", channel->GetName());
    playerBarWindow->SetCurrentChannel(channel);
    channelActivatedSignal(channel);
}
