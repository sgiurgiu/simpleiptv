#include "display_root_channel_group.h"

#include "display_channel.h"
#include "display_favourite_channel_group.h"

void DisplayRootChannelsGroup::renderGroup(SelectionSet& selectedNodes,
                                           const std::string& filter)
{
    // loadChildren();
    for (auto& g : children)
    {
        g->render(selectedNodes, filter);
    }
}

void DisplayRootChannelsGroup::setRoot(RootChannelsGroupPtr root,
                                       WorkersProvider* workersProvider,
                                       SimpleIPTVVulkan* vulkanInstance,
                                       const boost::asio::any_io_executor& ui_executor)
{
    children.clear();
    this->root = root;
    this->group = root;
    favouritesGroup = DisplayFavouritesChannelsGroup::Create(workersProvider,
                                                             ui_executor, this);
    root->IterateFavouriteChannels(
        [this, workersProvider, vulkanInstance, ui_executor](ChannelPtr channel)
        {
            auto dchannel =
                DisplayChannel::Create(channel, workersProvider, vulkanInstance,
                                       ui_executor, favouritesGroup.get());
            dchannel->indexInParent = favouritesGroup->children.size();
            dchannel->loadLogo();
            favouritesGroup->children.push_back(std::move(dchannel));
        });
    favouritesGroup->indexInParent = 0;
    children.push_back(favouritesGroup);
    loadChildren(workersProvider, vulkanInstance, ui_executor);

    // channels without a group are shown directly under the root, after all
    // the groups have been loaded
    root->IterateChannels(
        [this, workersProvider, vulkanInstance, ui_executor](ChannelPtr channel)
        {
            auto dchannel = DisplayChannel::Create(
                channel, workersProvider, vulkanInstance, ui_executor, this);
            dchannel->loadLogo();
            dchannel->indexInParent = children.size();
            children.push_back(std::move(dchannel));
        });
}

void DisplayRootChannelsGroup::loadChildren(
    WorkersProvider* workersProvider,
    SimpleIPTVVulkan* vulkanInstance,
    const boost::asio::any_io_executor& ui_executor)
{
    if (!root || !root->AreGroupsLoaded())
    {
        return;
    }

    if (children.size() < 2 && root->AreGroupsLoaded())
    {
        root->IterateGroups(
            [this, workersProvider, vulkanInstance,
             ui_executor](ChannelsGroupPtr group)
            {
                children.emplace_back(DisplayChannelsGroup::Create(
                    group, workersProvider, ui_executor, this));
                children.back().get()->indexInParent = children.size() - 1;
                children.back().get()->loadChildren(workersProvider,
                                                    vulkanInstance, ui_executor);
            });
    }
}

bool DisplayRootChannelsGroup::ActivateChannelOfGroup(ChannelsGroupPtr group,
                                                      ChannelPtr channel)
{
    DisplayNodeType groupType = DisplayNodeType::GROUP;
    if (group->GetId() < 0 && channel->IsFavourite())
    {
        // we got a special group
        groupType = DisplayNodeType::FAVOURITES;
    }

    auto findGroup = [group, groupType](DisplayNode* node) -> DisplayNode*
    {
        auto doFindNode = [&](auto& self, DisplayNode* node) -> DisplayNode*
        {
            DisplayNode* foundNode = nullptr;
            for (const auto& child : node->children)
            {
                if (child->getType() == groupType &&
                    child->getUnderlyingID() == group->GetId())
                {
                    foundNode = child.get();
                    break;
                }
                else
                {
                    foundNode = self(self, child.get());
                }
            }
            return foundNode;
        };
        return doFindNode(doFindNode, node);
    };

    auto foundGroup = findGroup(this);
    if (foundGroup)
    {
        foundGroup->isOpen = true;
        auto channelIt =
            std::find_if(foundGroup->children.begin(), foundGroup->children.end(),
                         [id = channel->GetId()](const auto& c)
                         {
                             return id == c->getUnderlyingID() &&
                                    c->getType() == DisplayNodeType::CHANNEL;
                         });
        if (channelIt != foundGroup->children.cend())
        {
            DisplayChannel* channel =
                dynamic_cast<DisplayChannel*>(channelIt->get());
            if (channel)
            {
                channel->activate();
                return true;
            }
        }
    }
    return false;
}
