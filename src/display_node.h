#pragma once

#include <GL/gl.h>
#include <imgui.h>

#include "channels/channel.h"
#include "channels/root_channel_group.h"

#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
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
    virtual void loadChildren(const boost::asio::any_io_executor& executor) = 0;
    virtual bool shouldRender(const std::string& filter) const = 0;
    DisplayNode* getNextNode(const boost::asio::any_io_executor& executor);
    DisplayNode* getPreviousNode(const boost::asio::any_io_executor& executor);

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
    void loadChildren(const boost::asio::any_io_executor& executor) override;
    bool shouldRender(const std::string& filter) const override;
    ChannelsGroupPtr group;
    float maxLogoWidth = 0.0;
};
struct DisplayChannel : public DisplayNode
{
    DisplayChannel(ChannelPtr channel,
                   DisplayChannelsGroup* parent,
                   const boost::asio::any_io_executor& executor);
    void render(std::unordered_set<DisplayNode*>& selectedNodes,
                const std::string& filter) override
    {
        renderChannel(selectedNodes, filter);
    }
    ~DisplayChannel();
    void renderChannel(std::unordered_set<DisplayNode*>& selectedNodes,
                       const std::string& filter);
    void loadChildren(const boost::asio::any_io_executor&) override
    {
    }
    bool shouldRender(const std::string& filter) const override;
    void loadLogoTexture();
    void decodeLogoImage(const boost::asio::any_io_executor& executor);
    void downloadLogoImage(const boost::asio::any_io_executor& executor);
    ChannelPtr channel;
    bool isActivated = false;
    GLuint channelLogoTexture = 0;
    std::atomic_int logoWidth = 0;
    std::atomic_int logoHeight = 0;
    std::atomic_int logoChannels = 0;
    std::atomic<ImVec2> displayLogoSize;
    std::atomic<unsigned char*> logoData = nullptr;
};
struct DisplayRootChannelsGroup : public DisplayChannelsGroup
{
    DisplayRootChannelsGroup() : DisplayChannelsGroup{ "", nullptr }
    {
    }
    void renderGroup(std::unordered_set<DisplayNode*>& selectedNodes,
                     const std::string& filter);
    void setRoot(RootChannelsGroupPtr root,
                 const boost::asio::any_io_executor& executor);
    void loadChildren(const boost::asio::any_io_executor& executor);
    RootChannelsGroupPtr root;
    std::unique_ptr<DisplayChannelsGroup> favouritesGroup;
};
struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
{
    DisplayFavouritesChannelsGroup(DisplayRootChannelsGroup* parent);
};
