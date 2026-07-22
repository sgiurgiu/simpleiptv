#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "channel.h"
#include "channels_group.h"
#include "root_channel_group.h"

namespace soci
{
class row;
}

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
    // the callback will be called on the cb_executor provided
    void LoadChannelsAndGroups(LoadRootCallback cb,
                               const boost::asio::any_io_executor& cb_executor);
    using LoadChannelsCallback =
        std::function<void(std::vector<ChannelPtr>, int)>;
    void GetFavouritesPage(int page,
                           int channelsPerPage,
                           LoadChannelsCallback cb,
                           const boost::asio::any_io_executor& cb_executor);
    void GetChannelsPage(ChannelsGroupPtr group,
                         int page,
                         int channelsPerPage,
                         LoadChannelsCallback cb,
                         const boost::asio::any_io_executor& cb_executor);
    using LoadGroupsCallback = std::function<void(std::vector<ChannelsGroupPtr>)>;
    void GetGroups(LoadGroupsCallback cb,
                   const boost::asio::any_io_executor& cb_executor);

    void UpdateChannelLogo(int id, std::string logo);
    void UpdateChannelLogoSync(int id, std::string logo);
    void UpdateChannelFavourite(int id, bool favourite);
    void UpdateChannelFavouriteSync(int id, bool favourite);
    using SaveGroupCallback = std::function<void(ChannelsGroupPtr)>;
    void SaveGroup(ChannelsGroupPtr group,
                   SaveGroupCallback cb,
                   const boost::asio::any_io_executor& cb_executor);
    // update or insert group and its children
    void UpsertGroup(ChannelsGroupPtr group,
                     SaveGroupCallback cb,
                     const boost::asio::any_io_executor& cb_executor);
    using SaveChannelCallback = std::function<void(ChannelPtr)>;
    // insert a single channel; if groupName is set and non-empty the group is
    // found-or-created, otherwise the channel is added with no group
    void AddChannel(ChannelPtr channel,
                    std::optional<std::string> groupName,
                    SaveChannelCallback cb,
                    const boost::asio::any_io_executor& cb_executor);

private:
    RootChannelsGroupPtr loadChannelsData();
    std::vector<ChannelPtr> loadFavourites();
    std::vector<ChannelsGroupPtr> loadGroups(std::optional<int> parentId);
    void loadGroup(ChannelsGroupPtr group,
                   const boost::asio::any_io_executor& cb_executor);
    std::vector<ChannelPtr> loadChannels(ChannelsGroupPtr group);
    ChannelPtr loadChannel(const soci::row&);
    ChannelsGroupPtr loadGroup(const soci::row&);

    std::map<int, ChannelsGroupPtr> loadAllGroups();
    std::vector<ChannelPtr> loadAllChannels();
    ChannelsGroupPtr findGroup(const std::string& name);
    ChannelPtr upsertChannel(ChannelPtr channel, ChannelsGroupPtr parent);

private:
    boost::asio::any_io_executor executor;
};
