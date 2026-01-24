#include "simpleiptv.h"

#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>

#ifdef STV_UNIX
#include "dbus_mpris_service.h"
#endif

namespace
{
static constexpr std::chrono::seconds ChannelsWindowTimerExpiry{ 5 };
} // namespace

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider* workersProvider,
                       SimpleIPTVVulkan* vulkanInstance,
                       MpvPlayer* mpvPlayer)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, vulkanInstance{ vulkanInstance }
, channelsWindow{ ChannelsWindow::Create(
      ui_executor, this->workersProvider, this->vulkanInstance) }
, playerBarWindow{ PlayerBarWindow::Create(
      ui_executor, this->workersProvider, this->vulkanInstance) }
, epgListingWindow{ EpgListingWindow::Create(ui_executor, this->workersProvider) }
, player{ mpvPlayer }
, channelsShowingTimer{ ui_executor }
{
    using namespace std::placeholders;
    channelsWindow->AddChannelActivatedListener(
        std::bind(&SimpleIPTV::channelActivated, this, _1));
    playerBarWindow->AddNextChannelListener(
        [this]() { channelsWindow->ActivateNextChannel(); });
    playerBarWindow->AddPreviousChannelListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    playerBarWindow->AddPauseChannelListener([this]() { player->Pause(); });
    playerBarWindow->AddPlayChannelListener([this]() { player->Play(); });
    playerBarWindow->AddStopChannelListener([this]() { player->Stop(); });
    playerBarWindow->AddVolumeListener([this](double vol)
                                       { player->SetVolume(vol); });
    player->AddFileLoadingErrorListener(
        [this](const std::string& error)
        { playerBarWindow->SetFileLoadingError(error); });
    player->AddVolumeListener(
        [this](double vol)
        {
#ifdef STV_UNIX
            this->workersProvider->GetMprisService()->SetVolume(vol);
#endif
            playerBarWindow->SetVolume(vol);
        });
    player->AddSubsAvailableListener(
        [this](std::vector<std::string> subsIds)
        { playerBarWindow->SetAvailableSubIds(std::move(subsIds)); });

    playerBarWindow->SetVolume(player->GetVolume());
    playerBarWindow->AddEpgListingButtonChangedListener(
        [this](bool pressed) { epgListingWindow->SetClosed(!pressed); });
    epgListingWindow->AddChannelActivatedListener(
        [this](ChannelsGroupPtr group, ChannelPtr channel)
        { channelsWindow->ActivateChannelOfGroup(group, channel); });
    playerBarWindow->AddCCButtonChangedListener([this](const std::string& id)
                                                { player->ClosedCaptions(id); });

    player->AddPlayerStateListener(
        [this](PlayerState state)
        { playerBarWindow->SetCurrentPlayerState(state); });

#ifdef STV_UNIX
    auto mprisService = workersProvider->GetMprisService();
    mprisService->AddNextListener([this]()
                                  { channelsWindow->ActivateNextChannel(); });
    mprisService->AddPreviousListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    mprisService->AddPlayListener([this]() { player->Play(); });
    mprisService->AddPauseListener([this]() { player->Pause(); });
    mprisService->AddStopListener([this]() { player->Stop(); });
    mprisService->AddPlayPauseListener([]() { /*player.PlayPause();*/ });
    mprisService->AddQuitListener([this]() { quit = true; });
    player->AddPlayerStateListener(
        [mprisService](PlayerState state)
        { mprisService->SetCurrentPlayerState(state); });
    mprisService->AddVolumeListener([this](double vol)
                                    { player->SetVolume(vol); });
    mprisService->SetVolume(player->GetVolume());
#endif
}

void SimpleIPTV::rearmChannelsShowingTimer()
{
    channelsShowingTimer.expires_after(ChannelsWindowTimerExpiry);
    channelsShowingTimer.async_wait(
        [this](auto ec)
        {
            if (!ec)
            {
                showChannels = false;
            }
        });
}

ImRect SimpleIPTV::showDesktop()
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
            player->VolumeToggleMute();
        }

        if (ImGui::GetIO().MouseWheel > 0)
        {
            player->VolumeIncrease();
        }
        if (ImGui::GetIO().MouseWheel < 0)
        {
            player->VolumeDecrease();
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

void SimpleIPTV::setSize(int width, int height)
{
    player->SetSize(width, height);
}

void SimpleIPTV::channelActivated(ChannelPtr channel)
{
    spdlog::debug("{} activated", channel->GetName());
    playerBarWindow->SetCurrentChannel(channel);
    player->Play(channel);
#ifdef STV_UNIX
    auto mprisService = workersProvider->GetMprisService();
    mprisService->SetCurrentChannel(channel);
#endif
}
