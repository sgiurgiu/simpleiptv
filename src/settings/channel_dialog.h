#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "../workers_provider.h"

class ChannelDialog : public std::enable_shared_from_this<ChannelDialog>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ChannelDialog(Key,
                  boost::asio::any_io_executor executor,
                  WorkersProvider* workersProvider);
    static std::shared_ptr<ChannelDialog> Create(
        boost::asio::any_io_executor executor, WorkersProvider* workersProvider);
    void ShowDialog();
    void SetShowAddChannelDialog();

    // notified (on the ui executor) whenever a channel is added
    template <typename S>
    void AddChannelsChangedListener(S slot)
    {
        channelsChangedSignal.connect(slot);
    }

private:
    void saveChannel();

private:
    boost::asio::any_io_executor ui_executor;
    WorkersProvider* workersProvider;
    bool showingDialog = false;
    std::string name;
    std::string uri;
    std::string logoUri;
    std::string groupName;
    bool favourite = false;
    std::vector<std::string> existingGroups;
    boost::signals2::signal<void()> channelsChangedSignal;
};
