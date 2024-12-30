#pragma once

#include <GL/gl.h>
#include <atomic>
#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <imgui.h>
#include <memory>
#include <vector>

#include "channels/channel.h"
#include "epg_listing.h"
#include "workers_provider.h"

class PlayerBarWindow : public std::enable_shared_from_this<PlayerBarWindow>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    PlayerBarWindow(Key,
                    const boost::asio::any_io_executor& ui_executor,
                    WorkersProvider* workersProvider);
    static std::shared_ptr<PlayerBarWindow>
    Create(const boost::asio::any_io_executor& executor,
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
    bool IsEpgListingPressed() const
    {
        return epgListingPressed;
    }
    void SetEpgListingPressed(bool flag)
    {
        epgListingPressed = flag;
    }
    template <typename S>
    void AddEpgListingButtonChangedListener(S slot)
    {
        epgListingButtonChangedSignal.connect(slot);
    }

private:
    void loadChannelLogoData();
    void loadEpg();

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
    bool epgListingPressed = false;
    GLuint channelLogoTexture = 0;
    ImVec2 channelLogoSize = { 0, 0 };
    std::string fileLoadingError;
    using VolumeSignal = boost::signals2::signal<void(double)>;
    VolumeSignal volumeSignal;
    std::vector<EpgListing> epgListings;
    std::atomic_bool loadingEpgs = false;
    using EpgListingButtonChangedSignal = boost::signals2::signal<void(bool)>;
    EpgListingButtonChangedSignal epgListingButtonChangedSignal;
};
