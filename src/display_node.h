#pragma once

#include "channels/channel.h"
#include "channels/root_channel_group.h"

#include <string>
#include <unordered_set>
#include <vector>

#include <boost/signals2.hpp>

struct DisplayChannel;
struct DisplayChannelsGroup;
struct DisplayNode
{
public:
    using ActivatedChannelSignal = boost::signals2::signal<void(DisplayChannel*)>;
    DisplayNode();
    DisplayNode(DisplayChannelsGroup* parent);
    DisplayNode(const std::string& name, DisplayChannelsGroup* parent);
    virtual ~DisplayNode() = default;

    virtual void render(std::unordered_set<DisplayNode*>& selectedNodes,
                        const std::string& filter) = 0;
    virtual void loadChildren() = 0;
    virtual bool shouldRender(const std::string& filter) = 0;
    DisplayNode* getNextNode();
    DisplayNode* getPreviousNode();

    DisplayChannelsGroup* parent = nullptr;
    int indexInParent = 0;
    std::string name;
    bool selected = false;
    bool isOpen = false;
    std::vector<std::unique_ptr<DisplayNode>> children;
    ActivatedChannelSignal activatedChannelSignal;
};
struct DisplayChannelsGroup : public DisplayNode
{
    DisplayChannelsGroup(const std::string& name, DisplayChannelsGroup* parent)
    : DisplayNode{ name, parent }
    {
    }
    DisplayChannelsGroup(ChannelsGroupPtr group, DisplayChannelsGroup* parent)
    : DisplayNode{ group->GetName(), parent }, group{ group }
    {
    }
    virtual ~DisplayChannelsGroup() = default;
    void render(std::unordered_set<DisplayNode*>& selectedNodes,
                const std::string& filter) override
    {
        renderGroup(selectedNodes, filter);
    }
    virtual void renderGroup(std::unordered_set<DisplayNode*>& selectedNodes,
                             const std::string& filter);
    void loadChildren() override;
    bool shouldRender(const std::string& filter) override;
    ChannelsGroupPtr group;
    bool openByDefault = false;
};
struct DisplayChannel : public DisplayNode
{
    DisplayChannel(ChannelPtr channel, DisplayChannelsGroup* parent)
    : DisplayNode{ channel->GetName(), parent }, channel{ channel }
    {
    }
    void render(std::unordered_set<DisplayNode*>& selectedNodes,
                const std::string& filter) override
    {
        renderChannel(selectedNodes, filter);
    }
    void renderChannel(std::unordered_set<DisplayNode*>& selectedNodes,
                       const std::string& filter);
    void loadChildren() override
    {
    }
    bool shouldRender(const std::string& filter) override;
    ChannelPtr channel;
    bool isActivated = false;
};
struct DisplayRootChannelsGroup : public DisplayChannelsGroup
{
    DisplayRootChannelsGroup() : DisplayChannelsGroup{ "", nullptr }
    {
    }
    void renderGroup(std::unordered_set<DisplayNode*>& selectedNodes,
                     const std::string& filter);
    void setRoot(RootChannelsGroupPtr root);
    void loadChildren();
    RootChannelsGroupPtr root;
    std::unique_ptr<DisplayChannelsGroup> favouritesGroup;
};
struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
{
    DisplayFavouritesChannelsGroup(DisplayRootChannelsGroup* parent);
};
