#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <memory>
#include <vector>

#include "../simpleiptv_vulkan.h"
#include "common.h"

enum class DisplayNodeType
{
    ROOT,
    FAVOURITES,
    GROUP,
    CHANNEL,
    SERVER,
    SERVER_CATEGORY,
    REMOTE_GROUP
};

struct DisplayChannelsGroup;
struct DisplayChannel;
class WorkersProvider;

struct DisplayNode : public std::enable_shared_from_this<DisplayNode>
{
protected:
    struct DisplayNodeKey
    {
    };

public:
    // The activated channel is passed as a shared_ptr so that whoever holds on
    // to it (ChannelsWindow keeps the currently playing one) cannot end up with
    // a raw pointer into a node the next tree reload frees.
    using ActivatedChannelSignal =
        boost::signals2::signal<void(std::shared_ptr<DisplayChannel>)>;
    using ReloadLocalChannelsSignal = boost::signals2::signal<void()>;
    DisplayNode(DisplayNodeKey);
    DisplayNode(DisplayNodeKey, DisplayChannelsGroup* parent);
    DisplayNode(DisplayNodeKey,
                const std::string& name,
                DisplayChannelsGroup* parent);
    virtual ~DisplayNode();

    virtual int getUnderlyingID() const = 0;
    virtual DisplayNodeType getType() const = 0;
    virtual void render(SelectionSet& selectedNodes,
                        const std::string& filter) = 0;
    virtual void loadChildren(WorkersProvider* workersProvider,
                              SimpleIPTVVulkan* vulkanInstance,
                              const boost::asio::any_io_executor& ui_executor) = 0;
    virtual bool shouldRender(const std::string& filter) const = 0;
    DisplayNode* getNextNode(WorkersProvider* workersProvider,
                             SimpleIPTVVulkan* vulkanInstance,
                             const boost::asio::any_io_executor& ui_executor);
    DisplayNode* getPreviousNode(WorkersProvider* workersProvider,
                                 SimpleIPTVVulkan* vulkanInstance,
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
    // The set this node is currently selected in, if any. Maintained by
    // SelectionSet so ~DisplayNode can deregister itself.
    SelectionSet* selectionSet = nullptr;
    std::vector<std::shared_ptr<DisplayNode>> children;
    ActivatedChannelSignal activatedChannelSignal;
    ReloadLocalChannelsSignal reloadLocalChannelsSignal;
};
