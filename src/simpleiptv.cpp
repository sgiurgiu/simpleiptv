#include "simpleiptv.h"

#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider& workersProvider)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(ui_executor, this->workersProvider) }
, player{ ui_executor, workersProvider }
, channelsShowingTimer{ ui_executor }
{
    setSize(1280, 720);
    player.InitializeMpvGL();
    using namespace std::placeholders;
    channelsWindow->addChannelActivatedListener(
        std::bind(&SimpleIPTV::channelActivated, this, _1));
}

void SimpleIPTV::setSize(int width, int height)
{
    this->width = width;
    this->height = height;
    player.SetSizeAsync(width, height);
}

void SimpleIPTV::rearmChannelsShowingTimer()
{
    channelsShowingTimer.expires_after(std::chrono::seconds(5));
    channelsShowingTimer.async_wait(
        [this](auto ec)
        {
            if (!ec)
            {
                if (ImGui::GetCurrentContext()->MouseStationaryTimer >= 5.f)
                {
                    showChannels = false;
                }
                else
                {
                    rearmChannelsShowingTimer();
                }
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

    if (player.GetPlayerState() != PlayerState::PLAYING)
    {
        showChannels = true;
    }
    else
    {
        if (showChannels &&
            channelsShowingTimer.expiry() < std::chrono::steady_clock::now())
        {
            showChannels = false;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            showChannels = true;
            rearmChannelsShowingTimer();
        }
    }
    if (showChannels)
    {
        channelsWindow->showWindow(true);
    }

    if (player.GetPlayerState() == PlayerState::PLAYING)
    {
        player.Render();
    }
}
void SimpleIPTV::channelActivated(ChannelPtr channel)
{
    spdlog::debug("{} activated", channel->GetName());
    player.Play(channel);
}
