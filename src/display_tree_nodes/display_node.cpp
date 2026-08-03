#include "display_node.h"

#include "display_channel_group.h"

DisplayNode::DisplayNode(DisplayNodeKey key) : DisplayNode{ key, nullptr }
{
}
DisplayNode::DisplayNode(DisplayNodeKey key, DisplayChannelsGroup* parent)
: DisplayNode{ key, "", parent }
{
}
DisplayNode::DisplayNode(DisplayNodeKey,
                         const std::string& name,
                         DisplayChannelsGroup* parent)
: parent{ parent }, name{ name }
{
    if (parent)
    {
        activatedChannelSignal.connect(parent->activatedChannelSignal);
    }
}

DisplayNode::~DisplayNode()
{
    // Leave no dangling pointer behind in the selection: nodes are routinely
    // destroyed while selected (channels reload, "Remove Favourite", server
    // refresh).
    if (selectionSet)
    {
        selectionSet->erase(this);
    }
}

DisplayNode* DisplayNode::getNextNode(WorkersProvider* workersProvider,
                                      SimpleIPTVVulkan* vulkanInstance,
                                      const boost::asio::any_io_executor& ui_executor)
{
    DisplayNode* curr_node = this;
    curr_node->loadChildren(workersProvider, vulkanInstance, ui_executor);
    if (!curr_node->children.empty())
    {
        isOpen = true;
        return curr_node->children.begin()->get();
    }
    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(workersProvider, vulkanInstance,
                                        ui_executor);
        if (curr_node->indexInParent + 1 < (int)curr_node->parent->children.size())
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent + 1).get();
            node->isOpen = true;
            if (!node->children.empty())
            {
                return node->children.begin()->get();
            }
            else
            {
                return node;
            }
        }
        curr_node = curr_node->parent;
    }
    return nullptr;
}

DisplayNode*
DisplayNode::getPreviousNode(WorkersProvider* workersProvider,
                             SimpleIPTVVulkan* vulkanInstance,
                             const boost::asio::any_io_executor& ui_executor)
{
    DisplayNode* curr_node = this;

    while (curr_node->parent != nullptr)
    {
        curr_node->parent->loadChildren(workersProvider, vulkanInstance,
                                        ui_executor);
        if (curr_node->indexInParent > 0)
        {
            auto node =
                curr_node->parent->children.at(curr_node->indexInParent - 1).get();
            node->loadChildren(workersProvider, vulkanInstance, ui_executor);
            node->isOpen = true;
            if (!node->children.empty())
            {
                return node->children.rbegin()->get();
            }
            else
            {
                return node;
            }
        }
        curr_node = curr_node->parent;
    }
    return nullptr;
}
