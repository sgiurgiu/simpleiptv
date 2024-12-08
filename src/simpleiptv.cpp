#include "simpleiptv.h"

#include <imgui.h>

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext,
                       WorkersProvider& workersProvider)
: ui_executor{ uiContext.get_executor() }
, workersProvider{ workersProvider }
, channelsWindow{ ChannelsWindow::Create(ui_executor, workersProvider) }
, player{ ui_executor }
{
    setSize(1280, 720);
    player.initializeMpvGL();
}

void SimpleIPTV::setSize(int width, int height)
{
    this->width = width;
    this->height = height;
    player.setSizeAsync(width, height);
}

void SimpleIPTV::showDesktop()
{
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Q)) &&
        ImGui::GetIO().KeyCtrl)
    {
        quit = true;
    }
    channelsWindow->showWindow();
    if (player.getPlayerState() == PlayerState::PLAYING)
    {
        player.render();
    }
}