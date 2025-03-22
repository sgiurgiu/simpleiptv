#pragma once

#include "../channels/root_channel_group.h"
#include "display_channel_group.h"

struct DisplayRootChannelsGroup : public DisplayChannelsGroup
{
    DisplayRootChannelsGroup(DisplayNodeKey key)
    : DisplayChannelsGroup{ key, "", nullptr }
    {
    }
    static std::shared_ptr<DisplayRootChannelsGroup> Create()
    {
        return std::make_shared<DisplayRootChannelsGroup>(DisplayNodeKey{});
    }
    void renderGroup(std::unordered_set<DisplayNode*>& selectedNodes,
                     const std::string& filter) override;
    void setRoot(RootChannelsGroupPtr root,
                 WorkersProvider* workersProvider,
                 const boost::asio::any_io_executor& ui_executor);
    void loadChildren(WorkersProvider* workersProvider,
                      const boost::asio::any_io_executor& ui_executor) override;
    void ActivateChannelOfGroup(ChannelsGroupPtr group, ChannelPtr channel);
    virtual int getUnderlyingID() const override
    {
        return -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::ROOT;
    }

    RootChannelsGroupPtr root;
    std::shared_ptr<DisplayChannelsGroup> favouritesGroup;
};
