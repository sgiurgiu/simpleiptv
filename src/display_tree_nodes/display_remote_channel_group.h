#pragma once

#include "display_channel_group.h"
#include "display_server_category.h"

#include <fmt/format.h>
#include <functional>
#include <optional>
#include <vector>

struct DisplayRemoteChannelsGroup : public DisplayChannelsGroup
{
    DisplayRemoteChannelsGroup(DisplayNodeKey key,
                               const std::string& name,
                               const std::string& url,
                               WorkersProvider* workersProvider,
                               const boost::asio::any_io_executor& ui_executor,
                               DisplayServerCategory* parent);
    static std::shared_ptr<DisplayRemoteChannelsGroup>
    Create(const std::string& name,
           const std::string& url,
           WorkersProvider* workersProvider,
           const boost::asio::any_io_executor& ui_executor,
           DisplayServerCategory* parent)
    {
        return std::make_shared<DisplayRemoteChannelsGroup>(
            DisplayNodeKey{}, name, url, workersProvider, ui_executor, parent);
    }

    virtual int getUnderlyingID() const override
    {
        return -1;
    }
    virtual DisplayNodeType getType() const override
    {
        return DisplayNodeType::REMOTE_GROUP;
    }
    virtual void render(std::unordered_set<DisplayNode*>& selectedNodes,
                        const std::string& filter) override;
    virtual void loadChildren(WorkersProvider*,
                              SimpleIPTVVulkan*,
                              const boost::asio::any_io_executor&) override
    {
    }
    virtual bool shouldRender(const std::string&) const override;
    void showPopup();
    // we have another one because we want to control here how we load our children
    void loadRemoteChildren();
    void saveGroupLocally();
    DisplayServerCategory* serverCategory;
    std::string url;
    std::string channelsFilter;
    std::string channelsFilterLabel;
    std::string eraserFilterLabel;
    bool areChildrenLoading = false;
    bool areChildrenLoaded = false;
    std::optional<std::string> error;
    using SaveGroupFunction = std::function<void()>;
    std::vector<SaveGroupFunction> saveGroupCallbacks;
};
