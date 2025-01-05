#pragma once

#include <GL/gl.h>
#include <imgui.h>

#include "channels/channel.h"
#include "channels/root_channel_group.h"

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>

struct DisplayChannel;
struct DisplayChannelsGroup;
class WorkersProvider;
enum class DisplayNodeType
{
    ROOT,
    FAVOURITES,
    GROUP,
    CHANNEL
};
struct DisplayNode : public std::enable_shared_from_this<DisplayNode>
{
protected:
    struct DisplayNodeKey
    {
    };

public:
    using ActivatedChannelSignal = boost::signals2::signal<void(DisplayChannel*)>;
    DisplayNode(DisplayNodeKey);
    DisplayNode(DisplayNodeKey, DisplayChannelsGroup* parent);
    DisplayNode(DisplayNodeKey,
                const std::string& name,
                DisplayChannelsGroup* parent);
    virtual ~DisplayNode() = default;

    virtual int getUnderlyingID() const = 0;
    virtual DisplayNodeType getType() const = 0;
    virtual void render(std::unordered_set<DisplayNode*>& selectedNodes,
                        const std::string& filter) = 0;
    virtual void loadChildren(WorkersProvider* workersProvider,
                              const boost::asio::any_io_executor& ui_executor) = 0;
    virtual bool shouldRender(const std::string& filter) const = 0;
    DisplayNode* getNextNode(WorkersProvider* workersProvider,
                             const boost::asio::any_io_executor& ui_executor);
    DisplayNode* getPreviousNode(WorkersProvider* workersProvider,
                                 const boost::asio::any_io_executor& ui_executor);

    template <typename Derived>
    std::shared_ptr<Derived> shared_from_base()
    {
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    DisplayChannelsGroup* parent = nullptr;
    int indexInParent = 0;
    std::string name;
    bool selected = false;
    bool isOpen = false;
    std::vector<std::shared_ptr<DisplayNode>> children;
    ActivatedChannelSignal activatedChannelSignal;
};
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
struct DisplayChannel : public DisplayNode
{
    DisplayChannel(DisplayNodeKey,
                   ChannelPtr channel,
                   DisplayChannelsGroup* parent);
    static std::shared_ptr<DisplayChannel> Create(ChannelPtr channel,
                                                  DisplayChannelsGroup* parent)
    {
        return std::make_shared<DisplayChannel>(DisplayNodeKey{}, channel,
                                                parent);
    }
    void render(std::unordered_set<DisplayNode*>& selectedNodes,
                const std::string& filter) override
    {
        renderChannel(selectedNodes, filter);
    }
    ~DisplayChannel();
    void loadLogo(WorkersProvider* workersProvider,
                  const boost::asio::any_io_executor& ui_executor);
    void renderChannel(std::unordered_set<DisplayNode*>& selectedNodes,
                       const std::string& filter);
    void loadChildren(WorkersProvider*, const boost::asio::any_io_executor&) override
    {
    }
    bool shouldRender(const std::string& filter) const override;
    void loadLogoTexture();
    void decodeLogoImage(const boost::asio::any_io_executor& executor);
    void decodeLogoImage();
    void downloadLogoImage(WorkersProvider* workersProvider,
                           const boost::asio::any_io_executor& ui_executor);
    virtual int getUnderlyingID() const override
    {
        return channel ? channel->GetId() : -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::CHANNEL;
    }
    void activate();

    ChannelPtr channel;
    bool isActivated = false;
    GLuint channelLogoTexture = 0;
    std::atomic_int logoWidth = 0;
    std::atomic_int logoHeight = 0;
    std::atomic_int logoChannels = 0;
    std::atomic<ImVec2> displayLogoSize;
    std::atomic<unsigned char*> logoData = nullptr;
    bool shouldScrollToChannel = false;
    float scrollY = 0.0;
};
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
struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
{
    DisplayFavouritesChannelsGroup(DisplayNodeKey,
                                   DisplayRootChannelsGroup* parent);
    static std::shared_ptr<DisplayFavouritesChannelsGroup>
    Create(DisplayRootChannelsGroup* parent)
    {
        return std::make_shared<DisplayFavouritesChannelsGroup>(DisplayNodeKey{},
                                                                parent);
    }
    virtual int getUnderlyingID() const override
    {
        return -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::FAVOURITES;
    }
};
