#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <imgui.h>

#include "channels/channel.h"

class PlayerBarWindow
{

public:
    PlayerBarWindow(const boost::asio::any_io_executor& ui_executor);

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

    void SetCurrentChannel(ChannelPtr channel)
    {
        currentChannel = channel;
    }
    bool IsChannelListPressed() const
    {
        return channelListPressed;
    }

private:
    boost::asio::any_io_executor ui_executor;
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
};
