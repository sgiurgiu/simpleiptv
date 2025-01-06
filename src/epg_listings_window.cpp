#include "epg_listings_window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include <algorithm>
#include <boost/asio/post.hpp>
#include <chrono>

#include "fonts/IconsFontAwesome4.h"

namespace
{
static constexpr auto COL_WIDTH_DURATION = std::chrono::hours{ 1 };
}

EpgListingWindow::EpgListingWindow(Key,
                                   const boost::asio::any_io_executor& ui_executor,
                                   WorkersProvider* workersProvider)
: ui_executor{ ui_executor }, workersProvider{ workersProvider }
{
    selectedGroup = std::make_shared<ChannelsGroup>(-1, "Loading Groups...",
                                                    std::optional<int>{});
    favouritesGroup = std::make_shared<ChannelsGroup>(
        -1, reinterpret_cast<const char*>(ICON_FA_STAR " Favourites"),
        std::optional<int>{});
    reloadCoveredHours();
}
void EpgListingWindow::reloadCoveredHours()
{
    coveredHours.clear();
    columnsStartPos.clear();
    auto tp =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    auto top_of_the_hour_tp = std::chrono::floor<std::chrono::hours>(tp);
    minCoveredHour = top_of_the_hour_tp;
    for (int i = 0; i < columnsCount - 1; i++)
    {
        coveredHours.push_back(top_of_the_hour_tp);
        top_of_the_hour_tp += COL_WIDTH_DURATION;
    }
    maxCoveredHour = top_of_the_hour_tp;
    columnsStartPos.resize(columnsCount);
}
bool EpgListingWindow::shouldReloadCoveredHours() const
{
    if (coveredHours.empty())
        return true;
    auto tp =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    auto top_of_the_hour_tp = std::chrono::floor<std::chrono::hours>(tp);
    return (coveredHours.begin()->time_since_epoch() !=
            top_of_the_hour_tp.time_since_epoch());
}
std::shared_ptr<EpgListingWindow>
EpgListingWindow::Create(const boost::asio::any_io_executor& executor,
                         WorkersProvider* workersProvider)
{
    return std::make_shared<EpgListingWindow>(Key{}, executor, workersProvider);
}
EpgListingWindow::~EpgListingWindow()
{
}
bool EpgListingWindow::ShowWindow()
{
    static const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
    auto mainViewport = ImGui::GetMainViewport();
    ImVec2 size;
    size.x = mainViewport->WorkSize.x * 0.8f;
    size.y =
        (mainViewport->WorkSize.y - ImGui::GetStyle().WindowBorderSize) * 0.6f;
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Program Guide", &open, windowFlags))
    {
        ImGui::End();
        return open;
    }

    if (shouldReloadCoveredHours())
    {
        reloadCoveredHours();
        loadChannelsOfSelectedGroup();
    }
    {
        auto tp = std::chrono::current_zone()->to_local(
            std::chrono::system_clock::now());
        currentLocalTime = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    }

    auto availableSpace = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(availableSpace / 3.f);
    if (ImGui::BeginCombo("Groups", selectedGroup->GetName().c_str(),
                          ImGuiComboFlags_WidthFitPreview))
    {
        for (auto& g : groups)
        {
            if (ImGui::Selectable(g->GetName().c_str(), g == selectedGroup))
            {
                page = 0;
                selectedGroup = g;
                loadChannelsOfSelectedGroup();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    static const int pageSizes[] = { 10, 20, 30 };
    static int pageSizesIndex = 0;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.f);
    if (ImGui::Combo("Channels/Page", &pageSizesIndex, " 10\0 20\0 30\0"))
    {
        channelsPerPage = pageSizes[pageSizesIndex];
        page = 0;
        loadChannelsOfSelectedGroup();
    }
    ImGui::SameLine();
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    bool shouldLeftBeDisabled = page <= 0;
    if (shouldLeftBeDisabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::ArrowButton("##left", ImGuiDir_Left))
    {
        page--;
        if (page < 0)
        {
            page = 0;
        }
        else
        {
            loadChannelsOfSelectedGroup();
        }
    }
    if (shouldLeftBeDisabled)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine(0.0f, spacing);
    ImGui::Text("Page %d/%d", page + 1, maxPages + 1);
    ImGui::SameLine(0.0f, spacing);
    bool shouldRightBeDisabled = page >= maxPages;
    if (shouldRightBeDisabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::ArrowButton("##right", ImGuiDir_Right))
    {
        page++;
        if (page > maxPages)
        {
            page = maxPages;
        }
        else
        {
            loadChannelsOfSelectedGroup();
        }
    }
    if (shouldRightBeDisabled)
    {
        ImGui::EndDisabled();
    }

    ImGui::BeginChild("channels_list");

    addHoursHeaderBar();

    for (const auto& c : channels)
    {
        addChannel(c);
    }
    ImGui::EndChild();

    ImGui::End();
    return open;
}

bool EpgListingWindow::addChannel(const DisplayChannelPtr& channel)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    float maxDisplayWidth = window->WorkRect.GetWidth();

    const float rowHeight = 50.f;
    auto pos = window->DC.CursorPos;
    const ImGuiID id = window->GetID(channel->channel->GetName().c_str());
    ImVec2 size =
        ImGui::CalcItemSize(ImVec2(maxDisplayWidth, rowHeight), 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav))
    {
        return false;
    }
    maxDisplayWidth = size.x;
    float colWidth = std::floor(maxDisplayWidth / (float)columnsCount);

    bool pressed = false;
    {
        ImVec2 maxListingVec = pos + ImVec2(colWidth, rowHeight);
        if (maxListingVec.y > window->ClipRect.Max.y ||
            pos.y <= window->ClipRect.Min.y)
        {
            return false;
        }

        drawList->PushClipRect(pos, maxListingVec);
        bool hovered = false;
        bool held = false;
        pressed = ImGui::ButtonBehavior(ImRect(pos, maxListingVec), id, &hovered,
                                        &held, ImGuiButtonFlags_PressedOnClick);
        ImU32 color = pressed
                          ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                          : (hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                     : 0xFF000000);
        drawList->AddRectFilled(pos, maxListingVec, color, 0);
        drawList->AddRect(pos, maxListingVec, color, 0);
        drawList->AddText(ImVec2(pos.x + ImGui::GetFontSize() / 2.f, pos.y + 2),
                          0xFFFFFFFF, channel->channel->GetName().c_str());
        drawList->PopClipRect();
        if (hovered)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (pressed)
            {
                open = false;
                channelActivatedSignal(selectedGroup, channel->channel);
            }
        }
    }
    pos += ImVec2(colWidth, 0.f);
    int index = 0;
    auto durPerPixelColWidth =
        (double)std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::hours{ 1 })
            .count() /
        colWidth;
    for (const auto& epg : channel->epgListings)
    {
        if (epg.GetStartTime() >= maxCoveredHour)
            break;

        auto startTime = epg.GetStartTime();
        auto endTime = epg.GetEndTime();
        if (startTime < minCoveredHour)
        {
            startTime = minCoveredHour;
        }
        if (endTime > maxCoveredHour)
        {
            endTime = maxCoveredHour;
        }
        auto gapFromStartDuration = startTime - minCoveredHour;
        float gapFromStartDurationCount =
            (float)std::chrono::duration_cast<std::chrono::system_clock::duration>(
                gapFromStartDuration)
                .count();
        float startX =
            std::floor(gapFromStartDurationCount / durPerPixelColWidth);
        auto gapFromEndDuration = endTime - minCoveredHour;
        float gapFromEndDurationCount =
            (float)std::chrono::duration_cast<std::chrono::system_clock::duration>(
                gapFromEndDuration)
                .count();
        float endX = std::floor(gapFromEndDurationCount / durPerPixelColWidth);

        ImRect listingRect;
        listingRect.Min = ImVec2{ startX + pos.x, pos.y };
        listingRect.Max = ImVec2{ endX + pos.x, pos.y + rowHeight };

        drawList->PushClipRect(listingRect.Min, listingRect.Max);
        bool hovered = false;
        bool held = false;
        const ImGuiID id = window->GetID(
            (channel->channel->GetName() + std::to_string(index)).c_str());
        ImGui::ButtonBehavior(listingRect, id, &hovered, &held);
        if (hovered && ImGui::BeginTooltip())
        {
            ImGui::Text("%s\n%s", epg.GetTimeAndProgram().c_str(),
                        epg.GetDescription().c_str());
            ImGui::EndTooltip();
        }
        ImU32 color =
            hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : 0xFF3D3837;

        drawList->AddRectFilled(listingRect.Min, listingRect.Max, color, 0);
        drawList->AddRect(listingRect.Min, listingRect.Max, 0xFFFFFFFF, 0);

        drawList->AddText(
            ImVec2(listingRect.Min.x + ImGui::GetFontSize() / 2.f, pos.y + 2),
            0xFFFFFFFF, epg.GetTitle().c_str());
        drawList->AddText(ImVec2(listingRect.Min.x + ImGui::GetFontSize() / 2.f,
                                 listingRect.Min.y + 2.f + ImGui::GetFontSize() +
                                     ImGui::GetStyle().FramePadding.y),
                          0xFFFFFFFF, epg.GetTime().c_str());

        drawList->PopClipRect();

        ++index;
    }
    return pressed;
}

bool EpgListingWindow::addHoursHeaderBar()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    float maxDisplayWidth = window->WorkRect.GetWidth();

    float headerHeight = ImGui::GetFontSize() +
                         ImGui::GetStyle().FramePadding.y * 2.f +
                         ImGui::GetStyle().FrameBorderSize;

    auto pos = window->DC.CursorPos;

    const ImGuiID id = window->GetID("time");
    ImVec2 size =
        ImGui::CalcItemSize(ImVec2(maxDisplayWidth, headerHeight), 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav))
    {
        return false;
    }
    maxDisplayWidth = size.x;
    float colWidth = std::floor(maxDisplayWidth / (float)columnsCount);
    ImVec2 maxListingVec = pos + ImVec2(colWidth, headerHeight);

    drawList->PushClipRect(pos, maxListingVec);
    drawList->AddRectFilled(pos, maxListingVec, 0xFF3D3837, 0);
    drawList->AddRect(pos, maxListingVec, 0xFFFFFFFF, 0);
    drawList->PopClipRect();
    int colsStartPosIndex = 0;
    columnsStartPos[colsStartPosIndex++].first = pos;

    for (const auto& time : coveredHours)
    {
        pos += ImVec2(colWidth, 0.f);
        auto format = std::format("{:%H:%M}", time);
        maxListingVec = pos + ImVec2(colWidth, headerHeight);
        drawList->PushClipRect(pos, maxListingVec);
        drawList->AddRectFilled(pos, maxListingVec, 0xFF3D3837, 0);
        drawList->AddRect(pos, maxListingVec, 0xFFFFFFFF, 0);
        drawList->AddText(ImVec2(pos.x + ImGui::GetFontSize() / 2.f, pos.y + 2),
                          0xFFFFFFFF, format.c_str());
        drawList->PopClipRect();
        columnsStartPos[colsStartPosIndex].first = pos;
        columnsStartPos[colsStartPosIndex++].second = time;
    }

    return true;
}

void EpgListingWindow::loadEpgs()
{
    groups.clear();
    using namespace std::placeholders;
    auto cb = std::bind(&EpgListingWindow::groupsLoaded, shared_from_this(), _1);
    workersProvider->GetChannelsRepository()->GetGroups(cb, ui_executor);
}

void EpgListingWindow::groupsLoaded(std::vector<ChannelsGroupPtr> groupPtrs)
{
    selectedGroup = favouritesGroup;
    groups.push_back(favouritesGroup);
    groups.insert(groups.end(), groupPtrs.cbegin(), groupPtrs.cend());
    loadChannelsOfSelectedGroup();
}

void EpgListingWindow::loadChannelsOfSelectedGroup()
{
    channels.clear();
    auto cb =
        [self = shared_from_this()](std::vector<ChannelPtr> channels, int total)
    {
        self->channels.resize(channels.size());
        std::transform(channels.cbegin(), channels.cend(), self->channels.begin(),
                       [](auto& c)
                       {
                           return std::make_shared<DisplayChannel>(
                               c, std::vector<EpgListing>{});
                       });

        self->totalChannels = total;
        self->loadEpgsOfLoadedChannels();
    };
    if (selectedGroup == favouritesGroup)
    {
        workersProvider->GetChannelsRepository()->GetFavouritesPage(
            page, channelsPerPage, cb, ui_executor);
    }
    else
    {
        workersProvider->GetChannelsRepository()->GetChannelsPage(
            selectedGroup, page, channelsPerPage, cb, ui_executor);
    }
}
void EpgListingWindow::loadEpgsOfLoadedChannels()
{
    columnsCount = INITIAL_COLUMNS_COUNT;
    channelsLoadedEpgs = 0;
    maxPages = totalChannels / channelsPerPage;
    for (const auto& channel : channels)
    {
        if (channel->channel->GetEPGChannelUri().empty())
        {
            continue;
        }

        workersProvider->GetNetworkResourceProvider()->GetResource(
            channel->channel->GetEPGChannelUri(), ui_executor,
            [weak = weak_from_this(), channel](std::string body, std::error_code ec)
            {
                auto self = weak.lock();
                if (!self)
                    return;
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
                    channel->epgListings.emplace_back(listingObject);
                }
            },
            false);
    }
}