#pragma once

#include "display_node.h"

#include "../channels/channels_group.h"

struct DisplayChannelsGroup : public DisplayNode
{
    DisplayChannelsGroup(DisplayNodeKey key,
                         const std::string& name,
                         WorkersProvider* workersProvider,
                         const boost::asio::any_io_executor& ui_executor,
                         DisplayChannelsGroup* parent)
    : DisplayNode{ key, name, parent }
    , workersProvider{ workersProvider }
    , ui_executor{ ui_executor }
    {
    }
    DisplayChannelsGroup(DisplayNodeKey key,
                         ChannelsGroupPtr group,
                         WorkersProvider* workersProvider,
                         const boost::asio::any_io_executor& ui_executor,
                         DisplayChannelsGroup* parent)
    : DisplayNode{ key, group->GetName(), parent }
    , group{ group }
    , workersProvider{ workersProvider }
    , ui_executor{ ui_executor }
    {
    }
    static std::shared_ptr<DisplayChannelsGroup>
    Create(const std::string& name,
           WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor,
           DisplayChannelsGroup* parent)
    {
        return std::make_shared<DisplayChannelsGroup>(
            DisplayNodeKey{}, name, workersProvider, ui_executor, parent);
    }
    static std::shared_ptr<DisplayChannelsGroup>
    Create(ChannelsGroupPtr group,
           WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor,
           DisplayChannelsGroup* parent)
    {
        return std::make_shared<DisplayChannelsGroup>(
            DisplayNodeKey{}, group, workersProvider, ui_executor, parent);
    }

    virtual ~DisplayChannelsGroup() = default;
    void render(std::unordered_set<DisplayNode*>& selectedNodes,
                const std::string& filter) override
    {
        renderGroup(selectedNodes, filter);
    }
    virtual void renderGroup(std::unordered_set<DisplayNode*>& selectedNodes,
                             const std::string& filter);
    void loadChildren(WorkersProvider* workersProvider,
                      const boost::asio::any_io_executor& ui_executor) override;
    bool shouldRender(const std::string& filter) const override;
    virtual int getUnderlyingID() const override
    {
        return group ? group->GetId() : -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::GROUP;
    }
    void removeChannel(std::shared_ptr<DisplayChannel> channel);
    void addChannel(ChannelPtr channel);
    ChannelsGroupPtr group;
    WorkersProvider* workersProvider;
    boost::asio::any_io_executor ui_executor;
    float maxLogoWidth = 0.0;
};
