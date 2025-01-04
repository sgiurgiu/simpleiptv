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
static constexpr std::chrono::milliseconds resizeDebounceDelay{ 100 };
} // namespace

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider* workersProvider)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(ui_executor, this->workersProvider) }
, playerBarWindow{ PlayerBarWindow::Create(ui_executor, this->workersProvider) }
, epgListingWindow{ EpgListingWindow::Create(ui_executor, this->workersProvider) }
, player{ ui_executor, this->workersProvider }
, channelsShowingTimer{ ui_executor }
{
    player.InitializeMpvGL();
    using namespace std::placeholders;
    channelsWindow->AddChannelActivatedListener(
        std::bind(&SimpleIPTV::channelActivated, this, _1));
    playerBarWindow->AddNextChannelListener(
        [this]() { channelsWindow->ActivateNextChannel(); });
    playerBarWindow->AddPreviousChannelListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    playerBarWindow->AddPauseChannelListener([this]() { player.Pause(); });
    playerBarWindow->AddPlayChannelListener([this]() { player.Play(); });
    playerBarWindow->AddStopChannelListener([this]() { player.Stop(); });
    playerBarWindow->AddVolumeListener([this](double vol)
                                       { player.SetVolume(vol); });
    player.AddFileLoadingErrorListener(
        [this](const std::string& error)
        { playerBarWindow->SetFileLoadingError(error); });
    player.AddVolumeListener(
        [this](double vol)
        {
#ifdef STV_UNIX
            this->workersProvider->GetMprisService()->SetVolume(vol);
#endif
            playerBarWindow->SetVolume(vol);
        });
    player.AddSubsAvailableListener(
        [this](std::vector<std::string> subsIds)
        { playerBarWindow->SetAvailableSubIds(std::move(subsIds)); });

    playerBarWindow->SetVolume(player.GetVolume());
    playerBarWindow->AddEpgListingButtonChangedListener(
        [this](bool pressed) { epgListingWindow->SetClosed(!pressed); });
    epgListingWindow->AddChannelActivatedListener(
        [this](ChannelsGroupPtr group, ChannelPtr channel)
        { channelsWindow->ActivateChannelOfGroup(group, channel); });
    playerBarWindow->AddCCButtonChangedListener([this](const std::string& id)
                                                { player.ClosedCaptions(id); });

#ifdef STV_UNIX
    auto mprisService = workersProvider->GetMprisService();
    mprisService->AddNextListener([this]()
                                  { channelsWindow->ActivateNextChannel(); });
    mprisService->AddPreviousListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    mprisService->AddPlayListener([this]() { player.Play(); });
    mprisService->AddPauseListener([this]() { player.Pause(); });
    mprisService->AddStopListener([this]() { player.Stop(); });
    mprisService->AddPlayPauseListener([]() { /*player.PlayPause();*/ });
    mprisService->AddQuitListener([this]() { quit = true; });
    player.AddPlayerStateListener(
        [mprisService](PlayerState state)
        { mprisService->SetCurrentPlayerState(state); });
    mprisService->AddVolumeListener([this](double vol)
                                    { player.SetVolume(vol); });
    mprisService->SetVolume(player.GetVolume());
#endif
}

void SimpleIPTV::setSize(int width, int height)
{
    if (width == this->width && height == this->height)
        return;

    auto now = std::chrono::steady_clock::now();
    if (now - lastResizeTime > resizeDebounceDelay)
    {
        this->width = width;
        this->height = height;

        player.SetSize(width, height);
        lastResizeTime = now;
    }
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

void SimpleIPTV::showDesktop()
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

    ImVec2 windowsSize = playerBarWindow->ShowWindow();
    if (playerBarWindow->IsChannelListPressed())
    {
        windowsSize.x = channelsWindow->ShowWindow(windowsSize.y).x;
    }
    else
    {
        windowsSize.x = 0.f;
    }
    if (!epgListingWindow->IsClosed())
    {
        epgListingWindow->ShowWindow();
    }
    else
    {
        playerBarWindow->SetEpgListingPressed(false);
    }

    if (player.GetPlayerState() == PlayerState::PLAYING)
    {
        player.Render(windowsSize);
    }

    if (!ImGui::IsAnyItemHovered() &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_M))
        {
            player.VolumeToggleMute();
        }

        if (ImGui::GetIO().MouseWheel > 0)
        {
            player.VolumeIncrease();
        }
        if (ImGui::GetIO().MouseWheel < 0)
        {
            player.VolumeDecrease();
        }
    }
}
void SimpleIPTV::channelActivated(ChannelPtr channel)
{
    spdlog::debug("{} activated", channel->GetName());
    playerBarWindow->SetCurrentChannel(channel);
    player.Play(channel);
#ifdef STV_UNIX
    auto mprisService = workersProvider->GetMprisService();
    mprisService->SetCurrentChannel(channel);
#endif
}
