#pragma once

#include "../channels/root_channel_group.h"
#include "../simpleiptv_vulkan.h"
#include "display_channel_group.h"

struct DisplayRootChannelsGroup : public DisplayChannelsGroup
{
    DisplayRootChannelsGroup(DisplayNodeKey key,
                             WorkersProvider* workersProvider,
                             const boost::asio::any_io_executor& ui_executor)
    : DisplayChannelsGroup{ key, "", workersProvider, ui_executor, nullptr }
    {
    }
    static std::shared_ptr<DisplayRootChannelsGroup>
    Create(WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor)
    {
        return std::make_shared<DisplayRootChannelsGroup>(
            DisplayNodeKey{}, workersProvider, ui_executor);
    }
    void renderGroup(SelectionSet& selectedNodes,
                     const std::string& filter) override;
    void setRoot(RootChannelsGroupPtr root,
                 WorkersProvider* workersProvider,
                 SimpleIPTVVulkan* vulkanInstance,
                 const boost::asio::any_io_executor& ui_executor);
    void loadChildren(WorkersProvider* workersProvider,
                      SimpleIPTVVulkan* vulkanInstance,
                      const boost::asio::any_io_executor& ui_executor) override;
    // Returns whether the channel was found in this tree and activated. It only
    // ever finds channels of the local tree, so callers that also deal with
    // server channels need a fallback.
    bool ActivateChannelOfGroup(ChannelsGroupPtr group, ChannelPtr channel);
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
