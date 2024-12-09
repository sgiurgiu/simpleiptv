#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <imgui.h>
#include <memory>

#include "channels/root_channel_group.h"
#include "serverpopup.h"
#include "workers_provider.h"

class ChannelsWindow : public std::enable_shared_from_this<ChannelsWindow>
{
private:
    struct Key
    {
        explicit Key() = default;
    };

public:
    ChannelsWindow(Key,
                   const boost::asio::any_io_executor& ui_executor,
                   WorkersProvider& workersProvider);
    static std::shared_ptr<ChannelsWindow>
    Create(const boost::asio::any_io_executor& executor,
           WorkersProvider& workersProvider);

    void showWindow(bool forceDisplay);
    bool shouldQuit() const
    {
        return quit;
    }

private:
    void loadLocalChannels();
    void showLocalChannelsTab();
    void showRemoteChannelsTab();
    void showMenu();

private:
    struct DisplayNode
    {
    public:
        DisplayNode();
        DisplayNode(DisplayNode* parent);
        DisplayNode(const std::string& name, DisplayNode* parent);
        virtual ~DisplayNode() = default;

        virtual void render() = 0;
        DisplayNode* getNextNode();
        void setNodesSelection(ImGuiSelectionBasicStorage* selection,
                               bool selected);
        int closeAndUnselectChildNodes(ImGuiSelectionBasicStorage* selection,
                                       int depth = 0);

        ImGuiID uid = 0;
        uintptr_t pointer = 0;
        DisplayNode* parent = nullptr;
        int indexInParent = 0;
        std::string name;
        std::vector<std::unique_ptr<DisplayNode>> children;
    };
    struct DisplayChannelsGroup;
    struct DisplayChannel : public DisplayNode
    {
        DisplayChannel(ChannelPtr channel, DisplayChannelsGroup* parent)
        : DisplayNode{ channel->GetName(), parent }, channel{ channel }
        {
        }
        void render() override
        {
            renderChannel();
        }
        void renderChannel();
        ChannelPtr channel;
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
        void render() override
        {
            renderGroup();
        }
        virtual void renderGroup();
        ChannelsGroupPtr group;
    };
    struct DisplayRootChannelsGroup : public DisplayChannelsGroup
    {
        DisplayRootChannelsGroup() : DisplayChannelsGroup{ "", nullptr }
        {
        }
        void applySelectionRequests(ImGuiMultiSelectIO* ms_io,
                                    ImGuiSelectionBasicStorage* selection);
        void renderGroup();
        void setRoot(RootChannelsGroupPtr root);
        RootChannelsGroupPtr root;
        std::unique_ptr<DisplayChannelsGroup> favouritesGroup;
    };
    struct DisplayFavouritesChannelsGroup : public DisplayChannelsGroup
    {
        DisplayFavouritesChannelsGroup(DisplayRootChannelsGroup* parent);
        void renderGroup();
    };

    boost::asio::any_io_executor ui_executor;
    WorkersProvider& workersProvider;
    float bgAlpha = 0.f;
    bool quit = false;
    DisplayRootChannelsGroup rootNode;
    std::string channelsFilter;
};
