#include "simpleiptv.h"

#include <imgui.h>

SimpleIPTV::SimpleIPTV(boost::asio::io_context& uiContext)
: ui_executor{ uiContext.get_executor() }
, channels{ ui_executor }
, player{ ui_executor }
{
    setSize(1280, 720);
    player.initializeMpvGL();
    // player.play("/home/sergiu/metallica_seattle.avi");
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
    channels.showWindow();
    if (player.getPlayerState() == PlayerState::PLAYING)
    {
        player.render();
    }
}