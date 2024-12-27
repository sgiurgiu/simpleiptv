#include "playerbar_window.h"

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include "fonts/IconsFontAwesome4.h"

PlayerBarWindow::PlayerBarWindow(const boost::asio::any_io_executor& ui_executor,
                                 WorkersProvider* workersProvider)
: ui_executor{ ui_executor }, workersProvider{ workersProvider }
{
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
        previousChannelSignal();
    }
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_PLAY)))
    {
        playChannelSignal();
    }
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STOP)))
    {
        stopChannelSignal();
    }
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_STEP_FORWARD)))
    {
        nextChannelSignal();
    }
    ImGui::SameLine();
    ImGui::Button(reinterpret_cast<const char*>(ICON_FA_VOLUME_UP));
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
        if (channelLogoTexture)
        {
            ImTextureID texture = static_cast<ImTextureID>(channelLogoTexture);
            ImGui::Image(texture, channelLogoSize);
            ImGui::SameLine();
        }
        ImGui::Text("%s", currentChannel->GetName().c_str());
        if (!epgListings.empty())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0x00000000);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0x00000000);

            auto text = fmt::format("{}-{} {}", epgListings[0].startHour,
                                    epgListings[0].endHour, epgListings[0].title);
            if (ImGui::BeginCombo("##EPG_Combo", text.c_str(),
                                  ImGuiComboFlags_WidthFitPreview))
            {
                for (const auto& l : epgListings)
                {
                    ImGui::Text("%s-%s %s", l.startHour.c_str(),
                                l.endHour.c_str(), l.title.c_str());
                }

                ImGui::EndCombo();
            }
            ImGui::PopStyleColor(5);
        }
    }
    else if (!fileLoadingError.empty())
    {
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "%s",
                           fileLoadingError.c_str());
    }

    auto localPosition = ImGui::GetCursorPosX();
    auto availableSpace = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availableSpace - localPosition -
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

    ImGui::End();
    return size;
}

void PlayerBarWindow::loadChannelLogoData()
{
    if (currentChannel->GetLogo().empty())
        return;
    int width = 0;
    int height = 0;
    int channels = 0;
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(currentChannel->GetLogo().data()),
        currentChannel->GetLogo().size(), &width, &height, &channels,
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
    loadChannelLogoData();
    loadEpg();
}

void PlayerBarWindow::loadEpg()
{
    if (!currentChannel->GetEPGChannelUri().empty())
    {
        workersProvider->GetNetworkResourceProvider()->GetResource(
            currentChannel->GetEPGChannelUri(), ui_executor,
            [this](std::string body, std::error_code ec)
            {
                if (ec)
                    return;
                auto json = nlohmann::json::parse(body, nullptr, false, true);
                if (json.is_discarded() || !json.is_object())
                {
                    // bad data
                    return;
                }
                auto epg_listings = json["epg_listings"];

                for (const auto& listingObject : epg_listings)
                {
                    EpgListing listing;
                    listing.id = listingObject["id"].get<std::string>();
                    listing.epgId = listingObject["epg_id"].get<std::string>();
                    listing.channelId =
                        listingObject["channel_id"].get<std::string>();
                    listing.streamId =
                        listingObject["stream_id"].get<std::string>();
                    listing.title =
                        decode64(listingObject["title"].get<std::string>());
                    listing.description =
                        decode64(listingObject["description"].get<std::string>());
                    listing.startTime = getTimePoint(
                        listingObject["start_timestamp"].get<std::string>());
                    listing.endTime = getTimePoint(
                        listingObject["stop_timestamp"].get<std::string>());
                    if (listing.startTime.time_since_epoch() ==
                        std::chrono::seconds{ 0 })
                    {
                        continue;
                    }
                    listing.startHour =
                        std::format("{:%H:%M}", listing.startTime);
                    listing.endHour = std::format("{:%H:%M}", listing.endTime);

                    epgListings.push_back(std::move(listing));
                }
            });
    }
}

std::chrono::local_time<std::chrono::nanoseconds>
PlayerBarWindow::getTimePoint(const std::string& timestamp)
{
    std::chrono::local_time<std::chrono::nanoseconds> chronoTime{
        std::chrono::seconds{ 0 }
    };
    time_t time = 0;
    auto [ptr, ec] = std::from_chars(timestamp.data(),
                                     timestamp.data() + timestamp.size(), time);
    if (ec == std::errc())
    {
        auto tz = std::chrono::current_zone();
        chronoTime = tz->to_local(std::chrono::system_clock::from_time_t(time));
    }
    return chronoTime;
}

std::string PlayerBarWindow::decode64(const std::string& val)
{
    using namespace boost::archive::iterators;
    using It =
        transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
    return boost::algorithm::trim_right_copy_if(
        std::string(It(std::begin(val)), It(std::end(val))),
        [](char c) { return c == '\0'; });
}