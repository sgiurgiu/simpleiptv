#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/signals2.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

#include "../simpleiptv_vulkan.h"

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
    using ActivatedChannelSignal = boost::signals2::signal<void(DisplayChannel*)>;
    using ReloadLocalChannelsSignal = boost::signals2::signal<void()>;
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
    std::vector<std::shared_ptr<DisplayNode>> children;
    ActivatedChannelSignal activatedChannelSignal;
    ReloadLocalChannelsSignal reloadLocalChannelsSignal;
};
