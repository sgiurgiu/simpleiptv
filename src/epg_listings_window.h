#pragma once

#include <boost/signals2.hpp>
#include <imgui.h>
#include <memory>
#include <vector>

#include "channels/channel.h"
#include "channels/channels_group.h"
#include "epg_listing.h"
#include "workers_provider.h"

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
    bool addHoursHeaderBar();
    bool addChannel(const DisplayChannelPtr& channel);

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
    int channelsLoadedEpgs = 0;
    using HoursTimePoint =
        std::chrono::time_point<std::chrono::local_t, std::chrono::milliseconds>;
    std::vector<HoursTimePoint> coveredHours;
    std::chrono::local_seconds currentLocalTime;
    HoursTimePoint maxCoveredHour;
    HoursTimePoint minCoveredHour;
    std::vector<std::pair<ImVec2, HoursTimePoint>> columnsStartPos;
    using ActivatedChannelSignal =
        boost::signals2::signal<void(ChannelsGroupPtr, ChannelPtr)>;
    ActivatedChannelSignal channelActivatedSignal;
};