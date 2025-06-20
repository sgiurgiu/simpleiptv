#pragma once

#include "display_channel_group.h"
#include "display_server_category.h"

struct DisplayRemoteChannelsGroup : public DisplayChannelsGroup
{
    DisplayRemoteChannelsGroup(DisplayNodeKey key,
                               const std::string& name,
                               const std::string& url,
                               WorkersProvider* workersProvider,
                               const boost::asio::any_io_executor& ui_executor,
                               DisplayServerCategory* parent)
    : DisplayChannelsGroup{ key, name, workersProvider, ui_executor, parent }
    , serverCategory{ parent }
    , url{ url }
    {
    }
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
                              const boost::asio::any_io_executor&) override
    {
    }
    virtual bool shouldRender(const std::string&) const override
    {
        return true;
    }
    // we have another one because we want to control here how we load our children
    void loadRemoteChildren();
    DisplayServerCategory* serverCategory;
    std::string url;
    bool areChildrenLoading = false;
};
