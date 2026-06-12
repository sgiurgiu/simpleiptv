#pragma once

#include <boost/signals2.hpp>
#include <chrono>
#include <cstdint>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "channels/channel.h"
#include "channels/channels_group.h"
#include "epg/epg_repository.h"
#include "epg_listing.h"
#include "workers_provider.h"

#if __cpp_lib_chrono < 201907L
#include <date/date.h>
#include <date/tz.h>
#endif

class EpgListingWindow : public std::enable_shared_from_this<EpgListingWindow>
{
private:
    struct Key
    {
    };
    static constexpr int INITIAL_COLUMNS_COUNT = 5;

public:
    EpgListingWindow(Key,
                     const boost::asio::any_io_executor& ui_executor,
                     WorkersProvider* workersProvider);
    static std::shared_ptr<EpgListingWindow>
    Create(const boost::asio::any_io_executor& executor,
           WorkersProvider* workersProvider);
    ~EpgListingWindow();
    bool ShowWindow();
    bool IsClosed() const
    {
        return !open;
    }
    void SetClosed(bool flag)
    {
        open = !flag;
        if (open)
        {
            loadEpgs();
        }
    }
    template <typename S>
    void AddChannelActivatedListener(S slot)
    {
        channelActivatedSignal.connect(slot);
    }

private:
    void loadEpgs();
    void groupsLoaded(std::vector<ChannelsGroupPtr> groupPtrs);
    void loadChannelsOfSelectedGroup();
    void loadEpgsOfLoadedChannels();
    void reloadCoveredHours();
    bool shouldReloadCoveredHours() const;

    struct DisplayChannel
    {
        ChannelPtr channel;
        std::vector<EpgListing> epgListings;
    };
    using DisplayChannelPtr = std::shared_ptr<DisplayChannel>;
    void loadEpgFromNetwork(const DisplayChannelPtr& channel);
    bool addHoursHeaderBar();
    bool addChannel(const DisplayChannelPtr& channel, std::size_t rowIndex);
    void showLiveEpgListing();
    void showSearchTab();
    void runSearch();
    bool addSearchResultCard(const EpgSearchResult& result);

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider = nullptr;
    bool open = false;
    std::vector<ChannelsGroupPtr> groups;
    std::vector<DisplayChannelPtr> channels;

    ChannelsGroupPtr selectedGroup;
    ChannelsGroupPtr favouritesGroup;
    int page = 0;
    int channelsPerPage = 10;
    int maxPages = 0;
    int totalChannels = 0;
    int columnsCount = INITIAL_COLUMNS_COUNT;
#if __cpp_lib_chrono >= 201907L
    using HoursTimePoint =
        std::chrono::time_point<std::chrono::local_t, std::chrono::milliseconds>;
    using LocalSeconds = std::chrono::local_seconds;
#else
    using HoursTimePoint = date::local_time<std::chrono::milliseconds>;
    using LocalSeconds = date::local_time<std::chrono::seconds>;
#endif
    static HoursTimePoint topOfCurrentHour();
    static LocalSeconds localNowSeconds();
    std::vector<HoursTimePoint> coveredHours;
    LocalSeconds currentLocalTime;
    HoursTimePoint maxCoveredHour;
    HoursTimePoint minCoveredHour;
    using ActivatedChannelSignal =
        boost::signals2::signal<void(ChannelsGroupPtr, ChannelPtr)>;
    ActivatedChannelSignal channelActivatedSignal;

    static constexpr int SEARCH_RESULT_LIMIT = 300;
    std::string searchText;
    std::vector<EpgSearchResult> searchResults;
    // Bumped on each search so a slow query's callback can be ignored once a
    // newer search has been issued.
    std::uint64_t searchGeneration = 0;
    // Distinguishes "no search run yet" from "search returned nothing".
    bool searchPerformed = false;
    // Selected entry in the Start/End time-range combos. Index 0 is "Anytime";
    // index N means N hours before/after now (see runSearch()).
    int startChoice = 0;
    int endChoice = 0;
};
