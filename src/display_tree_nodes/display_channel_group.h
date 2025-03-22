#pragma once

#include "display_node.h"

#include "../channels/channels_group.h"

struct DisplayChannelsGroup : public DisplayNode
{
    DisplayChannelsGroup(DisplayNodeKey key,
                         const std::string& name,
                         DisplayChannelsGroup* parent)
    : DisplayNode{ key, name, parent }
    {
    }
    DisplayChannelsGroup(DisplayNodeKey key,
                         ChannelsGroupPtr group,
                         DisplayChannelsGroup* parent)
    : DisplayNode{ key, group->GetName(), parent }, group{ group }
    {
    }
    static std::shared_ptr<DisplayChannelsGroup>
    Create(const std::string& name, DisplayChannelsGroup* parent)
    {
        return std::make_shared<DisplayChannelsGroup>(DisplayNodeKey{}, name,
                                                      parent);
    }
    static std::shared_ptr<DisplayChannelsGroup>
    Create(ChannelsGroupPtr group, DisplayChannelsGroup* parent)
    {
        return std::make_shared<DisplayChannelsGroup>(DisplayNodeKey{}, group,
                                                      parent);
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

    ChannelsGroupPtr group;
    float maxLogoWidth = 0.0;
};
