#include <GL/glew.h>

#include "playerbar_window.h"
#include "stv_utils.h"

#include <boost/asio/post.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include "fonts/IconsFontAwesome4.h"

PlayerBarWindow::PlayerBarWindow(Key,
                                 const boost::asio::any_io_executor& ui_executor,
                                 WorkersProvider* workersProvider)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, volumeIcon{ reinterpret_cast<const char*>(ICON_FA_VOLUME_UP) }
{
}
std::shared_ptr<PlayerBarWindow>
PlayerBarWindow::Create(const boost::asio::any_io_executor& executor,
                        WorkersProvider* workersProvider)
{
    return std::make_shared<PlayerBarWindow>(Key{}, executor, workersProvider);
}
PlayerBarWindow::~PlayerBarWindow()
{
    if (channelLogoTexture)
    {
        glDeleteTextures(1, &channelLogoTexture);
        channelLogoTexture = 0;
    }
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

    ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBackground);

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

    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STEP_BACKWARD)))
    {
        windowBackground = noChannelWindowBackground;
        previousChannelSignal();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(playerState == PlayerState::PLAYING || !currentChannel);
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_PLAY)))
    {
        windowBackground = noChannelWindowBackground;
        playChannelSignal();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(playerState != PlayerState::PLAYING);
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STOP)))
    {
        windowBackground = noChannelWindowBackground;
        stopChannelSignal();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STEP_FORWARD)))
    {
        windowBackground = noChannelWindowBackground;
        nextChannelSignal();
    }
    ImGui::SameLine();

    ImGui::Button(volumeIcon.c_str());
    bool isVolumeHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);
    if (isVolumeHovered || isVolumeSliderHovered ||
        (std::chrono::steady_clock::now() - lastVolumeHoveredTime <=
         std::chrono::seconds{ 2 }))
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::SliderInt("##vol_slider", &volume, 0, 150, "%d",
                             ImGuiSliderFlags_NoRoundToFormat))
        {
            volumeSignal(volume);
        }
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
        ImGui::BeginDisabled(subsIds.empty());
        if (ccButtonPressed)
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
        if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_CC)))
        {
            ccButtonPressed = !ccButtonPressed;
            std::string subId = "no";
            if (ccButtonPressed && !subsIds.empty())
            {
                subId = subsIds.at(0);
            }
            ccButtonChangedSignal(subId);
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (channelLogoTexture)
        {
            ImTextureID texture = static_cast<ImTextureID>(channelLogoTexture);
            ImGui::Image(texture, channelLogoSize);
            ImGui::SameLine();
        }
        ImGui::Text("%s", currentChannel->GetName().c_str());
        if (!epgListings.empty())
        {
            auto currentEpgIt =
                std::find_if(epgListings.begin(), epgListings.end(),
                             [](const auto& l) { return l.isListingCurrent(); });

            if (currentEpgIt != epgListings.end())
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0x00000000);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0x00000000);

                if (ImGui::BeginCombo("##EPG_Combo",
                                      currentEpgIt->GetTimeAndProgram().c_str(),
                                      ImGuiComboFlags_WidthFitPreview))
                {
                    for (const auto& l : epgListings)
                    {
                        ImGui::Text("%s", l.GetTimeAndProgram().c_str());
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor(5);
            }
        }
    }
    else if (!fileLoadingError.empty())
    {
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "%s",
                           fileLoadingError.c_str());
    }

    auto localPosition = ImGui::GetCursorPosX();
    auto availableSpace = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availableSpace - localPosition - (ImGui::GetFontSize() * 2.f) -
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

    ImGui::SetItemTooltip(
        "Show/Hide Channels window (Ctrl+Click anywhere to toggle)");

    ImGui::PopStyleColor(2);

    if (epgListingPressed)
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
    ImGui::SameLine();

    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_TELEVISION)))
    {
        epgListingPressed = !epgListingPressed;
        epgListingButtonChangedSignal(epgListingPressed);
    }
    ImGui::SetItemTooltip("Show/Hide Electronic Program Guide");

    ImGui::PopStyleColor(3);

    ImGui::End();
    return size;
}

void PlayerBarWindow::loadChannelLogoData()
{
    if (currentChannel->IsLogoEmpty())
        return;
    int width = 0;
    int height = 0;
    int channels = 0;
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(currentChannel->GetLogoData()),
        currentChannel->GetLogoSize(), &width, &height, &channels,
        STBI_rgb_alpha);

    float ratio = (float)width / (float)height;
    ImVec2 size{ ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.f +
                     ImGui::GetStyle().WindowPadding.y * 2.f,
                 ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.f +
                     ImGui::GetStyle().WindowPadding.y * 2.f };
    float area = size.x * size.y;
    size.x = std::sqrt(ratio * area);
    size.y = area / size.x;
    channelLogoSize = size;

    auto resizedImageData = stbir_resize_uint8_srgb(
        imageData, width, height, width * channels, nullptr, size.x, size.y,
        size.x * channels, (stbir_pixel_layout)channels);

    if (channelLogoTexture)
    {
        glDeleteTextures(1, &channelLogoTexture);
        channelLogoTexture = 0;
    }

    /*{
        auto [bg_r, bg_g, bg_b, _] = Utils::GetContrastColor(
            resizedImageData, (int)size.x, (int)size.y, channels);
        float s = 1.0f / 255.0f;
        windowBackground.x = bg_r * s;
        windowBackground.y = bg_g * s;
        windowBackground.z = bg_b * s;
    }*/

    glGenTextures(1, &channelLogoTexture);
    glBindTexture(GL_TEXTURE_2D, channelLogoTexture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE); // This is required on WebGL
                                       // for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE); // Same
    if (channels == 3)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, resizedImageData);
    }
    else if (channels == 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, resizedImageData);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(imageData);
    stbi_image_free(resizedImageData);
}

void PlayerBarWindow::SetCurrentChannel(ChannelPtr channel)
{
    currentChannel = channel;
    fileLoadingError = "";
    epgListings.clear();
    ccButtonPressed = false;
    subsIds.clear();
    if (channelLogoTexture)
    {
        glDeleteTextures(1, &channelLogoTexture);
        channelLogoTexture = 0;
    }
    windowBackground = noChannelWindowBackground;
    loadChannelLogoData();
    loadEpg(0);
}

void PlayerBarWindow::loadEpg(int retry)
{
    if (!currentChannel->GetEPGChannelUri().empty() && retry < 5)
    {
        workersProvider->GetNetworkResourceProvider()->GetResource(
            currentChannel->GetEPGChannelUri(), ui_executor,
            [weak = weak_from_this(), retry,
             channel = currentChannel](std::string body, std::error_code ec)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                if (self->currentChannel != channel)
                {
                    return;
                }
                if (ec)
                {
                    boost::asio::post(self->ui_executor, [self, retry]()
                                      { self->loadEpg(retry + 1); });
                    return;
                }
                auto json = nlohmann::json::parse(body, nullptr, false, true);
                if (json.is_discarded() || !json.is_object())
                {
                    // bad data
                    boost::asio::post(self->ui_executor, [self, retry]()
                                      { self->loadEpg(retry + 1); });
                    return;
                }
                auto epg_listings = json["epg_listings"];

                for (const auto& listingObject : epg_listings)
                {
                    self->epgListings.emplace_back(listingObject);
                }
            },
            false);
    }
}

void PlayerBarWindow::SetAvailableSubIds(std::vector<std::string> subsIds)
{
    this->subsIds = std::move(subsIds);
}
void PlayerBarWindow::SetVolume(double vol)
{
    volume = vol;
    if (volume <= 0.0)
    {
        volumeIcon = reinterpret_cast<const char*>(ICON_FA_VOLUME_OFF);
    }
    else
    {
        volumeIcon = reinterpret_cast<const char*>(ICON_FA_VOLUME_UP);
    }
}
