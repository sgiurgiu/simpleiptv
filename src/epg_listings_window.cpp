#include "epg_listings_window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_resize2.h>

#include <algorithm>
#include <array>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/post.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "fonts/IconsFontAwesome4.h"

namespace
{
static constexpr auto COL_WIDTH_DURATION = std::chrono::hours{ 1 };

// A small fixed palette the programs cycle through across the whole guide, so
// only a handful of colours are ever on screen. Even hue spacing keeps them
// distinct; the value is kept low enough for the white labels to stay readable.
const std::array<ImU32, 10>& listingPalette()
{
    static const std::array<ImU32, 10> palette = []
    {
        std::array<ImU32, 10> p{};
        for (std::size_t i = 0; i < p.size(); ++i)
        {
            float hue = static_cast<float>(i) / static_cast<float>(p.size());
            p[i] = ImColor::HSV(hue, 0.55f, 0.42f);
        }
        return p;
    }();
    return palette;
}

// Offsetting each row's starting colour by a stride coprime with the palette
// size staggers the colours so they don't line up into vertical stripes.
static constexpr std::size_t ROW_COLOR_STRIDE = 7;

// Pad a channel's (already sorted) listings with synthetic "No Data" entries so
// the whole covered window is always painted: before the first programme,
// between programmes, and after the last one. An empty channel becomes one big
// "No Data" block. Gaps are measured the same way the draw loop clamps overlaps
// (against the next programme's start), so the two stay consistent.
void fillNoDataGaps(std::vector<EpgListing>& listings,
                    EpgListing::LocalTime winStart,
                    EpgListing::LocalTime winEnd)
{
    if (winEnd <= winStart)
        return;

    std::vector<EpgListing> filled;
    filled.reserve(listings.size() * 2 + 1);

    if (listings.empty())
    {
        filled.push_back(EpgListing::MakeNoData(winStart, winEnd));
        listings = std::move(filled);
        return;
    }

    // gap before the first programme
    if (listings.front().GetStartTime() > winStart)
    {
        auto gapEnd = std::min(listings.front().GetStartTime(), winEnd);
        if (winStart < gapEnd)
            filled.push_back(EpgListing::MakeNoData(winStart, gapEnd));
    }

    for (std::size_t i = 0; i < listings.size(); ++i)
    {
        filled.push_back(listings[i]);

        // the gap after this programme runs to the next one, or to the end of
        // the window for the last programme
        auto gapStart = listings[i].GetEndTime();
        auto gapEnd =
            (i + 1 < listings.size()) ? listings[i + 1].GetStartTime() : winEnd;
        if (gapStart < gapEnd)
        {
            auto clampedStart = std::max(gapStart, winStart);
            auto clampedEnd = std::min(gapEnd, winEnd);
            if (clampedStart < clampedEnd)
                filled.push_back(EpgListing::MakeNoData(clampedStart, clampedEnd));
        }
    }

    listings = std::move(filled);
}

// Index of the local calendar day a listing starts on (days since the epoch),
// used to detect when consecutive search results cross into a new day.
std::int64_t localDayIndex(EpgListing::LocalTime time)
{
#if __cpp_lib_chrono >= 201907L
    return std::chrono::floor<std::chrono::days>(time).time_since_epoch().count();
#else
    return date::floor<date::days>(time).time_since_epoch().count();
#endif
}

// Human-friendly heading for the day a listing starts on: relative wording for
// the days around today, otherwise a full weekday/date.
std::string dayHeaderLabel(EpgListing::LocalTime time)
{
#if __cpp_lib_chrono >= 201907L
    using days = std::chrono::days;
    auto day = std::chrono::floor<days>(time);
    auto today = std::chrono::floor<days>(
        std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));
    if (day == today)
        return "Today";
    if (day == today + days{ 1 })
        return "Tomorrow";
    if (day == today - days{ 1 })
        return "Yesterday";
    return fmt::format("{:%A, %b %d}", day);
#else
    auto day = date::floor<date::days>(time);
    auto today = date::floor<date::days>(
        date::make_zoned(date::current_zone(), std::chrono::system_clock::now())
            .get_local_time());
    if (day == today)
        return "Today";
    if (day == today + date::days{ 1 })
        return "Tomorrow";
    if (day == today - date::days{ 1 })
        return "Yesterday";
    return date::format("%A, %b %d", day);
#endif
}
} // namespace

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
// The two timezone branches exist because some target compilers don't yet
// implement the C++20 <chrono> calendar/timezone API; there we fall back to
// Howard Hinnant's date library. Both produce the same local time points.
EpgListingWindow::HoursTimePoint EpgListingWindow::topOfCurrentHour()
{
#if __cpp_lib_chrono >= 201907L
    auto tp =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::chrono::floor<std::chrono::hours>(tp);
#else
    auto tp =
        date::make_zoned(date::current_zone(), std::chrono::system_clock::now())
            .get_local_time();
    return date::floor<std::chrono::hours>(tp);
#endif
}
EpgListingWindow::LocalSeconds EpgListingWindow::localNowSeconds()
{
#if __cpp_lib_chrono >= 201907L
    auto tp =
        std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::chrono::time_point_cast<std::chrono::seconds>(tp);
#else
    auto tp =
        date::make_zoned(date::current_zone(), std::chrono::system_clock::now())
            .get_local_time();
    return date::floor<std::chrono::seconds>(tp);
#endif
}
void EpgListingWindow::reloadCoveredHours()
{
    coveredHours.clear();
    auto top_of_the_hour_tp = topOfCurrentHour();
    minCoveredHour = top_of_the_hour_tp;
    for (int i = 0; i < columnsCount - 1; i++)
    {
        coveredHours.push_back(top_of_the_hour_tp);
        top_of_the_hour_tp += COL_WIDTH_DURATION;
    }
    maxCoveredHour = top_of_the_hour_tp;
}
bool EpgListingWindow::shouldReloadCoveredHours() const
{
    if (coveredHours.empty())
        return true;
    auto top_of_the_hour_tp = topOfCurrentHour();
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

    if (ImGui::BeginTabBar("EPGTabBar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Live"))
        {
            showLiveEpgListing();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Search"))
        {
            showSearchTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    return open;
}

void EpgListingWindow::showSearchTab()
{
    ImGui::Text("%s", reinterpret_cast<const char*>(ICON_FA_INFO_CIRCLE));
    ImGui::SetItemTooltip("%s", serverLastProgramGuideRefresh.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    if (ImGui::InputTextWithHint("##epgSearch", "Search title or description",
                                 &searchText,
                                 ImGuiInputTextFlags_EnterReturnsTrue))
    {
        runSearch();
    }
    ImGui::SameLine();
    if (ImGui::Button("Search"))
    {
        runSearch();
    }
    ImGui::SameLine();
    // Time-range filters. Index 0 is "Anytime"; index N is N hours
    // before/after now. The values are read back in runSearch().
    static const char* startItems =
        "Anytime\0Last 1 hour\0Last 2 hours\0Last 3 hours\0Last 4 hours\0"
        "Last 5 hours\0Last 6 hours\0Last 7 hours\0Last 8 hours\0Last 9 hours\0"
        "Last 10 hours\0";
    static const char* endItems =
        "Anytime\0Next 1 hour\0Next 2 hours\0Next 3 hours\0Next 4 hours\0"
        "Next 5 hours\0Next 6 hours\0Next 7 hours\0Next 8 hours\0Next 9 hours\0"
        "Next 10 hours\0";
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.f);
    if (ImGui::Combo("Start", &startChoice, startItems))
    {
        runSearch();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.f);
    if (ImGui::Combo("End", &endChoice, endItems))
    {
        runSearch();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu results", searchResults.size());
    ImGui::Separator();

    ImGui::BeginChild("search_results");
    if (!searchPerformed)
    {
        ImGui::TextDisabled("Type a title or description and press Enter.");
    }
    else if (searchResults.empty())
    {
        ImGui::TextDisabled("No matches.");
    }
    else
    {
        // Results arrive ordered by start time, so a single pass can drop a date
        // header in whenever the local day changes.
        std::int64_t prevDay = 0;
        bool first = true;
        for (const auto& result : searchResults)
        {
            std::int64_t day = localDayIndex(result.listing.GetStartTime());
            if (first || day != prevDay)
            {
                ImGui::SeparatorText(
                    dayHeaderLabel(result.listing.GetStartTime()).c_str());
                prevDay = day;
                first = false;
            }
            addSearchResultCard(result);
        }
    }
    ImGui::EndChild();
}

void EpgListingWindow::runSearch()
{
    auto query = boost::algorithm::trim_copy(searchText);
    if (query.empty())
    {
        searchResults.clear();
        searchPerformed = false;
        return;
    }
    auto now = std::chrono::system_clock::now();
    auto toUnix = [](std::chrono::system_clock::time_point tp)
    {
        return static_cast<int>(
            std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
                .count());
    };
    // Combo index 0 is "Anytime"; index N means N hours before/after now.
    auto start =
        (startChoice == 0) ? 0 : toUnix(now - std::chrono::hours(startChoice));
    auto end = (endChoice == 0) ? INT32_MAX
                                : toUnix(now + std::chrono::hours(endChoice));
    searchPerformed = true;
    auto gen = ++searchGeneration;
    workersProvider->GetEpgRepository()->SearchProgrammes(
        std::move(query), SEARCH_RESULT_LIMIT, start, end,
        [weak = weak_from_this(), gen](std::vector<EpgSearchResult> results)
        {
            auto self = weak.lock();
            if (!self)
                return;
            if (gen != self->searchGeneration)
                return; // a newer search has since been issued; drop this one
            self->searchResults = std::move(results);
        },
        ui_executor);
}

bool EpgListingWindow::addSearchResultCard(const EpgSearchResult& result)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    float maxDisplayWidth = window->WorkRect.GetWidth();

    const float fontSize = ImGui::GetFontSize();
    // Three stacked lines (channel + time, title, description) plus padding.
    const float rowHeight = fontSize * 3.f + ImGui::GetStyle().FramePadding.y * 6.f;

    auto pos = window->DC.CursorPos;
    const ImGuiID id = window->GetID(&result);
    ImVec2 size =
        ImGui::CalcItemSize(ImVec2(maxDisplayWidth, rowHeight), 0.0f, 0.0f);
    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav))
    {
        // Off-screen: ItemSize already advanced the cursor (keeping the
        // scrollbar correct), so there is nothing to draw.
        return false;
    }

    bool hovered = false;
    bool held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held,
                                         ImGuiButtonFlags_PressedOnClick);

    // A null channel means the programme's channel isn't in our list: it can be
    // shown (named from XMLTV) but not played from here.
    const bool playable = static_cast<bool>(result.channel);

    const auto& palette = listingPalette();
    // Key the colour off the channel so every result from the same channel
    // shares a colour, making the list easier to scan. Non-playable results key
    // off the name instead, since they have no channel id.
    std::size_t colorIndex =
        playable ? static_cast<std::size_t>(std::max(result.channel->GetId(),
                                                     0)) %
                       palette.size()
                 : std::hash<std::string>{}(result.channelName) % palette.size();
    ImU32 color = hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                          : palette[colorIndex];

    drawList->PushClipRect(bb.Min, bb.Max, true);
    drawList->AddRectFilled(bb.Min, bb.Max, color, 0);
    drawList->AddRect(bb.Min, bb.Max, 0xFFFFFFFF, 0);

    bool isNow = result.listing.isListingCurrent();
    if (isNow)
    {
        // Red left edge marks a programme that is on air right now.
        drawList->AddRectFilled(bb.Min, ImVec2(bb.Min.x + 4.f, bb.Max.y),
                                IM_COL32(255, 70, 70, 255), 0);
    }

    const float textX = bb.Min.x + fontSize / 2.f;
    const float lineGap = ImGui::GetStyle().FramePadding.y;
    float lineY = bb.Min.y + lineGap;

    // Line 1: channel name on the left, time range (and NOW) on the right.
    // Non-playable channels get a lock icon so the list still flags them.
    std::string nameLine;
    if (!playable)
        nameLine = reinterpret_cast<const char*>(ICON_FA_LOCK "  ");
    nameLine += result.channelName;
    drawList->AddText(ImVec2(textX, lineY), 0xFFFFFFFF, nameLine.c_str());
    const auto& timeStr = result.listing.GetTime();
    std::string rightStr = isNow ? ("NOW  " + timeStr) : timeStr;
    float rightWidth = ImGui::CalcTextSize(rightStr.c_str()).x;
    drawList->AddText(ImVec2(bb.Max.x - rightWidth - fontSize / 2.f, lineY),
                      isNow ? IM_COL32(255, 120, 120, 255)
                            : IM_COL32(220, 220, 220, 255),
                      rightStr.c_str());

    // Line 2: programme title.
    lineY += fontSize + lineGap;
    drawList->AddText(ImVec2(textX, lineY), 0xFFFFFFFF,
                      result.listing.GetTitle().c_str());

    // Line 3: description (clipped to the card).
    lineY += fontSize + lineGap;
    drawList->AddText(ImVec2(textX, lineY), IM_COL32(205, 205, 205, 255),
                      result.listing.GetDescription().c_str());

    drawList->PopClipRect();

    if (hovered)
    {
        if (playable)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::BeginTooltip())
        {
            ImGui::Text("%s\n%s\n%s", result.channelName.c_str(),
                        result.listing.GetTimeAndProgram().c_str(),
                        result.listing.GetDescription().c_str());
            if (!playable)
            {
                ImGui::Separator();
                ImGui::TextDisabled(
                    "Not in your channels — find it in the Channels list.");
            }
            ImGui::EndTooltip();
        }
        if (playable && pressed)
        {
            // Search has no selected group, so synthesise one matching the
            // channel's parent group; ActivateChannelOfGroup locates the channel
            // by group id within the channel tree.
            auto grp = std::make_shared<ChannelsGroup>(
                result.channel->GetParentId().value_or(-1), "",
                std::optional<int>{});
            open = false;
            channelActivatedSignal(grp, result.channel);
        }
    }
    return pressed;
}

void EpgListingWindow::showLiveEpgListing()
{
    if (shouldReloadCoveredHours())
    {
        reloadCoveredHours();
        loadChannelsOfSelectedGroup();
    }
    currentLocalTime = localNowSeconds();

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

    for (std::size_t row = 0; row < channels.size(); ++row)
    {
        addChannel(channels[row], row);
    }
    ImGui::EndChild();
}

bool EpgListingWindow::addChannel(const DisplayChannelPtr& channel,
                                  std::size_t rowIndex)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    float maxDisplayWidth = window->WorkRect.GetWidth();

    const float rowHeight = 50.f;
    auto pos = window->DC.CursorPos;
    const ImGuiID id = window->GetID(channel.get());
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
    auto durPerPixelColWidth =
        (double)std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::hours{ 1 })
            .count() /
        colWidth;
    const auto& listings = channel->epgListings;
    const auto& palette = listingPalette();
    for (std::size_t index = 0; index < listings.size(); ++index)
    {
        const auto& epg = listings[index];
        if (epg.GetStartTime() >= maxCoveredHour)
            break;

        auto startTime = epg.GetStartTime();
        auto endTime = epg.GetEndTime();
        if (startTime < minCoveredHour)
        {
            startTime = minCoveredHour;
        }
        // Some feeds contain an erroneous program whose duration runs far too
        // long and overlaps the entries after it. Clamp its end to the next
        // program's start so a bad entry can't swallow its neighbours.
        if (index + 1 < listings.size())
        {
            auto nextStart = listings[index + 1].GetStartTime();
            if (nextStart < endTime)
            {
                endTime = nextStart;
            }
        }
        if (endTime > maxCoveredHour)
        {
            endTime = maxCoveredHour;
        }
        if (endTime <= startTime)
        {
            continue; // fully overlapped or out of view, nothing to draw
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
        const ImGuiID id = window->GetID(&epg);
        ImGui::ButtonBehavior(listingRect, id, &hovered, &held);
        if (hovered && ImGui::BeginTooltip())
        {
            ImGui::Text("%s\n%s", epg.GetTimeAndProgram().c_str(),
                        epg.GetDescription().c_str());
            ImGui::EndTooltip();
        }
        ImU32 color;
        if (hovered)
        {
            color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        }
        else if (epg.IsNoData())
        {
            color = IM_COL32(45, 45, 48, 255); // recessed grey for gaps
        }
        else
        {
            std::size_t colorIndex =
                (rowIndex * ROW_COLOR_STRIDE + index) % palette.size();
            color = palette[colorIndex];
        }

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
    const ImVec2 headerOrigin = pos;

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

    for (const auto& time : coveredHours)
    {
        pos += ImVec2(colWidth, 0.f);
#if __cpp_lib_chrono >= 201907L
        auto format = fmt::format("{:%H:%M}", time);
#else
        auto format = date::format("%H:%M", time);
#endif
        maxListingVec = pos + ImVec2(colWidth, headerHeight);
        drawList->PushClipRect(pos, maxListingVec);
        drawList->AddRectFilled(pos, maxListingVec, 0xFF3D3837, 0);
        drawList->AddRect(pos, maxListingVec, 0xFFFFFFFF, 0);
        drawList->AddText(ImVec2(pos.x + ImGui::GetFontSize() / 2.f, pos.y + 2),
                          0xFFFFFFFF, format.c_str());
        drawList->PopClipRect();
    }

    // Place a vertical "now" marker inside the header bar (so it indicates the
    // current time without drawing over the rows below). It always falls in the
    // first column, since the covered range starts at the top of the hour.
    constexpr auto columnSpan =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            COL_WIDTH_DURATION);
    auto sinceColumnStart =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            currentLocalTime - minCoveredHour);
    float fraction = (float)sinceColumnStart.count() / (float)columnSpan.count();
    float nowX = headerOrigin.x + colWidth + colWidth * fraction;
    drawList->AddLine(ImVec2(nowX, headerOrigin.y),
                      ImVec2(nowX, headerOrigin.y + headerHeight),
                      IM_COL32(255, 70, 70, 255.f * 0.80f), 2.0f);

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
    maxPages = totalChannels > 0 ? (totalChannels - 1) / channelsPerPage : 0;

    // The repository is keyed in UTC seconds. Anchor the query window on "now"
    // (the same instant in any timezone) with a margin generous enough to keep
    // programmes overlapping either edge of the displayed hours; the draw loop
    // clips to the visible range precisely.
    auto now = std::chrono::system_clock::now();
    auto toUnix = [](std::chrono::system_clock::time_point tp)
    {
        return static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
                .count());
    };
    std::int64_t fromUnix = toUnix(now - std::chrono::hours{ 6 });
    std::int64_t windowEndUnix =
        toUnix(now + std::chrono::hours{ columnsCount + 2 });

    for (const auto& channel : channels)
    {
        if (channel->channel->GetEPGChannelId().empty())
        {
            // Nothing stored locally for this channel; try the internet (which
            // itself falls back to "No Data" when there is no EPG URI either).
            loadEpgFromNetwork(channel);
            continue;
        }

        workersProvider->GetEpgRepository()->GetProgrammes(
            channel->channel->GetXStreamServerId(),
            channel->channel->GetEPGChannelId(), fromUnix, windowEndUnix,
            [weak = weak_from_this(), channel](std::vector<EpgListing> listings)
            {
                auto self = weak.lock();
                if (!self)
                    return;
                if (listings.empty())
                {
                    // No stored programmes: fall back to the internet as before.
                    self->loadEpgFromNetwork(channel);
                    return;
                }
                // The repository already returns them ordered by start time.
                channel->epgListings = std::move(listings);
                fillNoDataGaps(channel->epgListings, self->minCoveredHour,
                               self->maxCoveredHour);
            },
            ui_executor);
    }
}

void EpgListingWindow::loadEpgFromNetwork(const DisplayChannelPtr& channel)
{
    if (channel->channel->GetEPGChannelUri().empty())
    {
        // No EPG source at all: show the whole window as "No Data".
        fillNoDataGaps(channel->epgListings, minCoveredHour, maxCoveredHour);
        return;
    }

    workersProvider->GetNetworkResourceProvider()->GetResource(
        channel->channel->GetEPGChannelUri(), ui_executor,
        [weak = weak_from_this(), channel](std::string body, std::error_code ec)
        {
            auto self = weak.lock();
            if (!self)
                return;
            if (!ec)
            {
                auto json = nlohmann::json::parse(body, nullptr, false, true);
                if (!json.is_discarded() && json.is_object())
                {
                    auto epg_listings = json["epg_listings"];
                    for (const auto& listingObject : epg_listings)
                    {
                        channel->epgListings.emplace_back(listingObject);
                    }
                    // The draw loop assumes chronological order (and clamps
                    // overlaps against the next entry), so keep them sorted.
                    std::sort(channel->epgListings.begin(),
                              channel->epgListings.end(),
                              [](const EpgListing& a, const EpgListing& b)
                              { return a.GetStartTime() < b.GetStartTime(); });
                }
            }
            // Whether the request failed, returned malformed data, or just
            // left gaps, pad the timeline so the window is fully covered.
            fillNoDataGaps(channel->epgListings, self->minCoveredHour,
                           self->maxCoveredHour);
        },
        false);
}

void EpgListingWindow::loadRemoteServers()
{
    workersProvider->GetServersRepository()->LoadServers(
        [weak = weak_from_this()](std::vector<ServerPtr> servers)
        {
            auto self = weak.lock();
            if (!self)
                return;
            self->serverLastProgramGuideRefresh = "EPG data refresh:\n";
            for (const auto& s : servers)
            {
                auto epgUpdatedAt = s->GetXmlTvUpdatedAt();
                if (epgUpdatedAt)
                {
#if __cpp_lib_chrono >= 201907L
                    auto tz = std::chrono::current_zone();
                    auto localTime = tz->to_local(*epgUpdatedAt);
#else
                    localTime =
                        date::make_zoned(date::current_zone(), *epgUpdatedAt)
                            .get_local_time();
#endif

                    self->serverLastProgramGuideRefresh.append(fmt::format(
                        "{} - {:%F %H:%M}\n", s->GetHost(), localTime));
                }
                else
                {
                    self->serverLastProgramGuideRefresh.append(
                        fmt::format("{}: Never\n", s->GetHost()));
                }
            }
        },
        ui_executor);
}
