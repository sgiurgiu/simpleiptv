#pragma once

#include <imgui.h>

#include "../channels/channel.h"
#include "display_node.h"

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
