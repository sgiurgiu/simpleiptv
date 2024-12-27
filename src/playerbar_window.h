#pragma once

#include <GL/gl.h>
#include <atomic>
#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <imgui.h>
#include <vector>

#include "channels/channel.h"
#include "workers_provider.h"

class PlayerBarWindow
{

public:
    PlayerBarWindow(const boost::asio::any_io_executor& ui_executor,
                    WorkersProvider* workersProvider);
    ~PlayerBarWindow();
    ImVec2 ShowWindow();
    bool IsPinned() const
    {
        return pinned;
    }

    template <typename S>
    void AddPreviousChannelListener(S slot)
    {
        previousChannelSignal.connect(slot);
    }
    template <typename S>
    void AddNextChannelListener(S slot)
    {
        nextChannelSignal.connect(slot);
    }
    template <typename S>
    void AddPauseChannelListener(S slot)
    {
        pauseChannelSignal.connect(slot);
    }
    template <typename S>
    void AddPlayChannelListener(S slot)
    {
        playChannelSignal.connect(slot);
    }
    template <typename S>
    void AddStopChannelListener(S slot)
    {
        stopChannelSignal.connect(slot);
    }

    void SetCurrentChannel(ChannelPtr channel);
    bool IsChannelListPressed() const
    {
        return channelListPressed;
    }
    void SetChannelListPressed(bool flag)
    {
        channelListPressed = flag;
    }
    void SetFileLoadingError(const std::string& error)
    {
        fileLoadingError = error;
    }
    void SetVolume(double vol)
    {
        volume = vol;
    }
    template <typename S>
    void AddVolumeListener(S slot)
    {
        volumeSignal.connect(slot);
    }

private:
    void loadChannelLogoData();
    void loadEpg();
    std::string decode64(const std::string& val);
    std::chrono::local_time<std::chrono::nanoseconds>
    getTimePoint(const std::string& timestamp);
    struct EpgListing
    {
        std::string id;
        std::string epgId;
        std::string title;
        std::string description;
        std::string channelId;
        std::string streamId;
        std::chrono::local_time<std::chrono::nanoseconds> startTime;
        std::chrono::local_time<std::chrono::nanoseconds> endTime;
        std::string startHour;
        std::string endHour;
    };

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    float bgAlpha = 0.6f;
    ChannelPtr currentChannel;
    std::string channelsFilter;

    using BasicOperationSignal = boost::signals2::signal<void()>;
    BasicOperationSignal previousChannelSignal;
    BasicOperationSignal nextChannelSignal;
    BasicOperationSignal pauseChannelSignal;
    BasicOperationSignal playChannelSignal;
    BasicOperationSignal stopChannelSignal;
    bool pinned = false;
    int volume = 50;
    bool isVolumeSliderHovered = false;
    std::chrono::steady_clock::time_point lastVolumeHoveredTime;
    bool channelListPressed = true;
    GLuint channelLogoTexture = 0;
    ImVec2 channelLogoSize = { 0, 0 };
    std::string fileLoadingError;
    using VolumeSignal = boost::signals2::signal<void(double)>;
    VolumeSignal volumeSignal;
    std::vector<EpgListing> epgListings;
    std::atomic_bool loadingEpgs = false;
};
