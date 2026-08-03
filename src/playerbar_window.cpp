#include "playerbar_window.h"
#include "stv_utils.h"

#include <boost/asio/post.hpp>
#include <cmath>
#include <cstdint>
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
                                 WorkersProvider* workersProvider,
                                 SimpleIPTVVulkan* vulkanInstance)
: ui_executor{ ui_executor }
, workersProvider{ workersProvider }
, vulkanInstance{ vulkanInstance }
, volumeIcon{ reinterpret_cast<const char*>(ICON_FA_VOLUME_UP) }
{
}
std::shared_ptr<PlayerBarWindow>
PlayerBarWindow::Create(const boost::asio::any_io_executor& executor,
                        WorkersProvider* workersProvider,
                        SimpleIPTVVulkan* vulkanInstance)
{
    return std::make_shared<PlayerBarWindow>(Key{}, executor, workersProvider,
                                             vulkanInstance);
}
PlayerBarWindow::~PlayerBarWindow()
{
    if (logo.tex)
    {
        vulkanInstance->WaitForIdle();
        vulkanInstance->DestroyImageData(logo);
        logo.tex = nullptr;
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
        if (logo.tex)
        {
            ImTextureID texture = reinterpret_cast<ImTextureID>(logo.tex);
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
    // Snapshot under Channel's lock; the logo can be replaced by the download
    // callback on another thread while we decode.
    const std::string encodedLogo = currentChannel->GetLogoCopy();
    if (encodedLogo.empty())
        return;
    int width = 0;
    int height = 0;
    int channels = 0;
    constexpr int kChannels = 4; // STBI_rgb_alpha => always RGBA
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(encodedLogo.data()), encodedLogo.size(),
        &width, &height, &channels, STBI_rgb_alpha);

    if (!imageData || width <= 0 || height <= 0)
    {
        // Corrupt/empty logo: bail instead of feeding null + NaN-derived
        // dimensions into stbir (UB + heap overflow).
        if (imageData)
            stbi_image_free(imageData);
        return;
    }

    float ratio = (float)width / (float)height;
    ImVec2 size{ ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.f +
                     ImGui::GetStyle().WindowPadding.y * 2.f,
                 ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.f +
                     ImGui::GetStyle().WindowPadding.y * 2.f };
    float area = size.x * size.y;
    size.x = std::sqrt(ratio * area);
    size.y = area / size.x;
    channelLogoSize = size;

    // Integer output dimensions used consistently for the resize stride and the
    // texture upload, so the buffer size can't disagree with what's read.
    int outW = static_cast<int>(std::lround(size.x));
    int outH = static_cast<int>(std::lround(size.y));
    if (outW < 1)
        outW = 1;
    if (outH < 1)
        outH = 1;

    auto resizedImageData = stbir_resize_uint8_srgb(
        imageData, width, height, width * kChannels, nullptr, outW, outH,
        outW * kChannels, (stbir_pixel_layout)kChannels);

    stbi_image_free(imageData);

    if (!resizedImageData)
        return;

    if (logo.tex)
    {
        vulkanInstance->WaitForIdle();
        vulkanInstance->DestroyPlayerBarImageData(logo);
    }

    logo = vulkanInstance->CreatePlayerBarImageData(outW, outH, kChannels,
                                                    resizedImageData);
    stbi_image_free(resizedImageData);
}

void PlayerBarWindow::SetCurrentChannel(ChannelPtr channel)
{
    currentChannel = channel;
    fileLoadingError = "";
    epgListings.clear();
    ccButtonPressed = false;
    subsIds.clear();
    if (logo.tex)
    {
        vulkanInstance->WaitForIdle();
        vulkanInstance->DestroyPlayerBarImageData(logo);
        logo.tex = nullptr;
        logo.name = "";
    }
    windowBackground = noChannelWindowBackground;
    loadChannelLogoData();
    loadEpg(0);
}

void PlayerBarWindow::loadEpg(int retry)
{
    // Prefer the locally imported guide; only when nothing is stored for this
    // channel do we go to the network. The network retry path re-enters through
    // loadEpgFromNetwork, so the database is only consulted on the first try.
    if (currentChannel && !currentChannel->GetEPGChannelId().empty())
    {
        // The combo lists the day's programmes and highlights the current one,
        // so query a window that brackets "now" with room on either side; the
        // repository is keyed in UTC seconds.
        auto now = std::chrono::system_clock::now();
        auto toUnix = [](std::chrono::system_clock::time_point tp)
        {
            return static_cast<std::int64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    tp.time_since_epoch())
                    .count());
        };
        std::int64_t fromUnix = toUnix(now - std::chrono::hours{ 6 });
        std::int64_t windowEndUnix = toUnix(now + std::chrono::hours{ 24 });

        workersProvider->GetEpgRepository()->GetProgrammes(
            currentChannel->GetXStreamServerId(),
            currentChannel->GetEPGChannelId(), fromUnix, windowEndUnix,
            [weak = weak_from_this(),
             channel = currentChannel](std::vector<EpgListing> listings)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                if (self->currentChannel != channel)
                    return;
                if (listings.empty())
                {
                    // Nothing stored locally: fall back to the network.
                    self->loadEpgFromNetwork(0);
                    return;
                }
                self->epgListings = std::move(listings);
            },
            ui_executor);
        return;
    }

    loadEpgFromNetwork(retry);
}

void PlayerBarWindow::loadEpgFromNetwork(int retry)
{
    if (!currentChannel->GetEPGChannelUri().empty() && retry < 5)
    {
        spdlog::info("Loading epg for channel {}, retry {}. URL used: {}",
                     currentChannel->GetName(), retry,
                     currentChannel->GetEPGChannelUri());
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
                    spdlog::error("Failed retrieving EPG for channel {}, retry "
                                  "{}. URL used: {}, with error: ",
                                  channel->GetName(), retry,
                                  channel->GetEPGChannelUri(), ec.message());
                    boost::asio::post(self->ui_executor, [self, retry]()
                                      { self->loadEpgFromNetwork(retry + 1); });
                    return;
                }
                auto json = nlohmann::json::parse(body, nullptr, false, true);
                if (json.is_discarded() || !json.is_object())
                {
                    spdlog::error("Failed retrieving EPG for channel {}, retry "
                                  "{}. URL used: {}, with error: ",
                                  channel->GetName(), retry,
                                  channel->GetEPGChannelUri(),
                                  " json discarded or json is not object");
                    // bad data
                    boost::asio::post(self->ui_executor, [self, retry]()
                                      { self->loadEpgFromNetwork(retry + 1); });
                    return;
                }
                auto epg_listings = json["epg_listings"];
                spdlog::info("Success loaded epg for channel {}, retry {}. URL "
                             "used: {}, listings count: {}",
                             channel->GetName(), retry,
                             channel->GetEPGChannelUri(), epg_listings.size());
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
