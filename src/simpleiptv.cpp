#include "simpleiptv.h"

#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>

namespace
{
static constexpr std::chrono::seconds ChannelsWindowTimerExpiry{ 5 };
static constexpr std::chrono::milliseconds resizeDebounceDelay{ 100 };
} // namespace

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider& workersProvider)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(ui_executor, this->workersProvider) }
, playerBarWindow{ ui_executor }
, player{ ui_executor, workersProvider }
, channelsShowingTimer{ ui_executor }
{
    setSize(1280, 720);
    player.InitializeMpvGL();
    using namespace std::placeholders;
    channelsWindow->AddChannelActivatedListener(
        std::bind(&SimpleIPTV::channelActivated, this, _1));
    playerBarWindow.AddNextChannelListener(
        [this]() { channelsWindow->ActivateNextChannel(); });
    playerBarWindow.AddPreviousChannelListener(
        [this]() { channelsWindow->ActivatePreviousChannel(); });
    playerBarWindow.AddPauseChannelListener([this]() { player.Pause(); });
    playerBarWindow.AddPlayChannelListener([this]() { player.Play(); });
    playerBarWindow.AddStopChannelListener([this]() { player.Stop(); });
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
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Q)) &&
        ImGui::GetIO().KeyCtrl)
    {
        quit = true;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl)
    {
        playerBarWindow.SetChannelListPressed(
            !playerBarWindow.IsChannelListPressed());
    }

    ImVec2 windowsSize = playerBarWindow.ShowWindow();
    if (playerBarWindow.IsChannelListPressed())
    {
        windowsSize.x = channelsWindow->ShowWindow(windowsSize.y).x;
    }
    else
    {
        windowsSize.x = 0.f;
    }

    if (player.GetPlayerState() == PlayerState::PLAYING)
    {
        player.Render(windowsSize);
    }

    if (!ImGui::IsAnyItemHovered() &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_M)))
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
    playerBarWindow.SetCurrentChannel(channel);
    player.Play(channel);
}
