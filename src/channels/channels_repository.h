#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <functional>
#include <memory>
#include <optional>

#include "root_channel_group.h"

class ChannelsRepository : public std::enable_shared_from_this<ChannelsRepository>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ChannelsRepository(Key, const boost::asio::any_io_executor& executor);
    static std::shared_ptr<ChannelsRepository>
    Create(const boost::asio::any_io_executor& executor);

    using LoadRootCallback = std::function<void(RootChannelsGroupPtr)>;
    void LoadChannelsAndGroups(LoadRootCallback cb);

private:
    RootChannelsGroupPtr loadAllChannels();
    std::vector<ChannelPtr> loadFavourites();
    std::vector<ChannelsGroupPtr> loadGroups(std::optional<int> parentId);
    void loadGroup(ChannelsGroupPtr group);
    std::vector<ChannelPtr> loadChannels(ChannelsGroupPtr group);

private:
    boost::asio::any_io_executor executor;
};