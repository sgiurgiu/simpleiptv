#include "simpleiptv.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider& workersProvider)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(ui_executor, this->workersProvider) }
, player{ ui_executor, workersProvider }
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

void SimpleIPTV::showDesktop()
{
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Q)) &&
        ImGui::GetIO().KeyCtrl)
    {
        quit = true;
    }
    channelsWindow->showWindow(player.GetPlayerState() != PlayerState::PLAYING);
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
