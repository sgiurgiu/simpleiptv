#pragma once

#include "display_channel_group.h"
#include "display_server_node.h"

#include <optional>

struct DisplayServerCategory : public DisplayChannelsGroup
{
    DisplayServerCategory(DisplayNodeKey key,
                          const std::string& name,
                          const std::string& url,
                          WorkersProvider* workersProvider,
                          const boost::asio::any_io_executor& ui_executor,
                          DisplayServer* parent);
    static std::shared_ptr<DisplayServerCategory>
    Create(const std::string& name,
           const std::string& url,
           WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor,
           DisplayServer* parent)
    {
        return std::make_shared<DisplayServerCategory>(
            DisplayNodeKey{}, name, url, workersProvider, ui_executor, parent);
    }

    virtual int getUnderlyingID() const override
    {
        return -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::SERVER_CATEGORY;
    }
    virtual void render(std::unordered_set<DisplayNode*>& selectedNodes,
                        const std::string&) override;
    virtual void loadChildren(WorkersProvider*,
                              SimpleIPTVVulkan*,
                              const boost::asio::any_io_executor&) override
    {
    }
    virtual bool shouldRender(const std::string&) const override
    {
        return true;
    }
    // we have another one because we want to control here how we load our children
    void loadRemoteChildren();
    DisplayServer* displayServer;
    std::string url;
    bool areChildrenLoading = false;
    bool areChildrenLoaded = false;
    std::optional<std::string> error;
    std::string groupsFilter;
    std::string groupsFilterLabel;
    std::string eraserLabel;
};
