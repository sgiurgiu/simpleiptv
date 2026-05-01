#include "display_channel.h"

#include <algorithm>
#include <boost/asio/post.hpp>

#include <cassert>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include "../workers_provider.h"
#include "display_channel_group.h"
#include "display_node.h"
#include "display_root_channel_group.h"

namespace
{
void clearSelectedNodes(std::unordered_set<DisplayNode*>& selectedNodes)
{
    for (DisplayNode* node : selectedNodes)
    {
        node->selected = false;
    }
    selectedNodes.clear();
}
} // namespace

DisplayChannel::DisplayChannel(DisplayNodeKey key,
                               ChannelPtr channel,
                               WorkersProvider* workersProvider,
                               SimpleIPTVVulkan* vulkanInstance,
                               const boost::asio::any_io_executor& ui_executor,
                               DisplayChannelsGroup* parent)
: DisplayNode{ key, channel->GetName(), parent }
, channel{ channel }
, workersProvider{ workersProvider }
, ui_executor{ ui_executor }
, displayLogoSize{ ImVec2{ (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.x * 2.f,
                           (ImGui::GetFontSize() * 2.f / 3.f) +
                               ImGui::GetStyle().FramePadding.y * 2.f } }
, vulkanInstance{ vulkanInstance }
{
}
void DisplayChannel::loadLogo()
{
    if (!channel->IsLogoEmpty())
    {
        decodeLogoImage(workersProvider->GetNetworkExecutor());
    }
    else
    {
        downloadLogoImage(workersProvider, ui_executor);
    }
}
void DisplayChannel::decodeLogoImage(const boost::asio::any_io_executor& executor)
{
    if (!channel->IsLogoEmpty())
    {
        boost::asio::post(
            executor,
            [weak = this->weak_from_this()]()
            {
                auto selfNode = weak.lock();
                if (!selfNode)
                    return;
                auto self = std::static_pointer_cast<DisplayChannel>(selfNode);
                self->decodeLogoImage();
            });
    }
}
void DisplayChannel::decodeLogoImage()
{
    int width = 0;
    int height = 0;
    int channels = 0;
    auto imageData = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(channel->GetLogoData()),
        channel->GetLogoSize(), &width, &height, &channels, STBI_rgb_alpha);

    float ratio = (float)width / (float)height;
    ImVec2 size = displayLogoSize;
    float area = size.x * size.y;
    size.x = std::sqrt(ratio * area);
    size.y = area / size.x;
    displayLogoSize = size;

    auto resizedImageData = stbir_resize_uint8_srgb(
        imageData, width, height, width * channels, nullptr, size.x, size.y,
        size.x * channels, (stbir_pixel_layout)channels);

    stbi_image_free(imageData);

    // Utils::EnhanceLogo(resizedImageData, size.x, size.y, channels);

    logoWidth = size.x;
    logoHeight = size.y;
    logoData = resizedImageData;
    logoChannels = channels;
}
void DisplayChannel::downloadLogoImage(WorkersProvider* workersProvider,
                                       const boost::asio::any_io_executor& ui_executor)
{
    if (!channel->IsLogoEmpty() || channel->GetLogoUri().empty())
        return;

    /**
     * We just need a thread (a different one than the UI thread and the network
     * threads) to get the callback into. We, therefore chose the DB thread as
     * it kinda seems like a waste to have a thread that will only be used once
     * in the lifetime of the application (when first downloading) the channels
     * logos. We can't use the network threads pool because those will be used
     * at maximum when we're downloading thousands of logos from the internet,
     * so the callback(s) will only be called very late in the process. We can't
     * use the UI thread because then we cannot play any channel while the logos
     * are downloading. It would make for a shitty first experience for the
     * user. So, we're just using the DB thread here as the callback receiver
     * (workersProvider->GetDBExecutor()).
     */
    workersProvider->GetNetworkResourceProvider()->GetResource(
        channel->GetLogoUri(), workersProvider->GetDBExecutor(),
        [weak = weak_from_this(), workersProvider,
         ui_executor](std::string logo, std::error_code ec)
        {
            auto selfNode = weak.lock();
            if (!selfNode)
                return;
            auto self = std::static_pointer_cast<DisplayChannel>(selfNode);
            if (ec)
            {
                spdlog::error("Cannot download '{}', failed with error: {}",
                              self->channel->GetLogoUri(), ec.message());
                return;
            }
            spdlog::debug("Downloaded logo for {}, from {}",
                          self->channel->GetName(), self->channel->GetLogoUri());
            // Logo Get<thing>/Set is protected by a mutex in Channel
            self->channel->SetLogo(logo);
            workersProvider->GetChannelsRepository()->UpdateChannelLogoSync(
                self->channel->GetId(), std::move(logo));
            self->decodeLogoImage();
        });
}
DisplayChannel::~DisplayChannel()
{
    if (logo.tex)
    {
        vulkanInstance->WaitForIdle();
        vulkanInstance->DestroyImageData(logo);
        logo.tex = nullptr;
    }
    if (logoData)
    {
        stbi_image_free(logoData);
    }
}

void DisplayChannel::activate()
{
    activatedChannelSignal(this);
    isActivated = true;
    shouldScrollToChannel = true;
    spdlog::debug("activated {} - {}", name, channel->GetUri());
}

bool DisplayChannel::shouldRender(const std::string& filter) const
{
    if (filter.empty())
        return true;
    auto it = std::search(name.cbegin(), name.cend(), filter.cbegin(),
                          filter.cend(), [](char c1, char c2)
                          { return std::tolower(c1) == std::tolower(c2); });
    return it != name.cend();
}

void DisplayChannel::renderChannel(std::unordered_set<DisplayNode*>& selectedNodes,
                                   const std::string& filter)
{
    if (!shouldRender(filter))
    {
        return;
    }

    const bool isSelected = selected;

    ImGuiSelectableFlags flags =
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
    if (isActivated && !isSelected)
    {
        flags |= ImGuiSelectableFlags_Highlight;

        ImGui::PushStyleColor(
            ImGuiCol_HeaderHovered,
            ImVec4(103.f / 255.f, 135.f / 255.f, 104.f / 255.f, 1.f));
    }
    ImGui::PushID(this);
    if (ImGui::Selectable("##channel", isSelected, flags))
    {
        selected = !selected;
        if (ImGui::GetIO().KeyCtrl)
        {
            // just this item changed selection
            if (selected)
            {
                selectedNodes.insert(this);
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
        else if (ImGui::GetIO().KeyShift)
        {
            // TODO: this is tricky, so leave it for later
            if (selected)
            {
                selectedNodes.insert(this);
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
        else
        {
            // if we were selected, and toggled
            if (!selected && !selectedNodes.empty())
                selected = true;

            if (selected)
            {
                clearSelectedNodes(selectedNodes);
                selectedNodes.insert(this);
                selected = true;
            }
            else
            {
                selectedNodes.erase(this);
            }
        }
    }
    showPopup(selectedNodes);
    if (shouldScrollToChannel)
    {
        ImGui::ScrollToItem(ImGuiScrollFlags_None);
        shouldScrollToChannel = false;
    }
    ImGui::PopID();
    if (isActivated && !isSelected)
    {
        ImGui::PopStyleColor(1);
    }
    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        activate();
    }
    ImGui::SameLine();
    loadLogoTexture();
    if (logo.tex)
    {
        ImVec2 size = displayLogoSize;
        ImVec2 dummySize{ ImGui::GetStyle().ItemSpacing.x, size.y };
        if (size.x < parent->maxLogoWidth)
        {
            dummySize.x += parent->maxLogoWidth - size.x;
        }
        ImTextureID texture = reinterpret_cast<ImTextureID>(logo.tex);
        ImGui::Image(texture, displayLogoSize);
        ImGui::SameLine(0.f, dummySize.x);
    }
    ImGui::Text("%s", channel->GetName().c_str());
    scrollY = ImGui::GetScrollY();
}
void DisplayChannel::loadLogoTexture()
{
    if (logoData && !logo.tex && vulkanInstance)
    {
        ImVec2 size = displayLogoSize;
        if (parent->maxLogoWidth < size.x)
        {
            parent->maxLogoWidth = size.x;
        }
        logo = vulkanInstance->CreateImageData(logoWidth, logoHeight,
                                               logoChannels, logoData);
    }
}

void DisplayChannel::showPopup(std::unordered_set<DisplayNode*>& selectedNodes)
{
    if (ImGui::BeginPopupContextItem())
    {
        clearSelectedNodes(selectedNodes);
        selectedNodes.insert(this);
        selected = true;
        bool favourite = parent->getType() == DisplayNodeType::FAVOURITES &&
                         channel->IsFavourite();
        if (!favourite && ImGui::Selectable("Add Favourite"))
        {
            // set favourite && add to favourites parent
            workersProvider->GetChannelsRepository()->UpdateChannelFavourite(
                channel->GetId(), true);
            DisplayChannelsGroup* p = parent;
            while (p->parent != nullptr)
            {
                p = p->parent;
            }
            assert(p != nullptr && p->getType() == DisplayNodeType::ROOT);
            DisplayRootChannelsGroup* root =
                static_cast<DisplayRootChannelsGroup*>(p);
            root->favouritesGroup->addChannel(channel, vulkanInstance);
        }
        if (favourite && ImGui::Selectable("Remove Favourite"))
        {
            workersProvider->GetChannelsRepository()->UpdateChannelFavourite(
                channel->GetId(), false);
            clearSelectedNodes(selectedNodes);
            boost::asio::post(ui_executor,
                              [self = shared_from_base<DisplayChannel>()]()
                              {
                                  auto p = self->parent;
                                  p->removeChannel(self);
                              });
        }
        ImGui::EndPopup();
    }
}
