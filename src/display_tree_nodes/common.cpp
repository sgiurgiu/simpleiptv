#include "common.h"

#include "display_node.h"

SelectionSet::~SelectionSet()
{
    // Should not happen (the set is declared to outlive its tree), but a node
    // left pointing at a destroyed set would dereference it in ~DisplayNode.
    for (DisplayNode* node : nodes)
    {
        node->selectionSet = nullptr;
    }
}

void SelectionSet::insert(DisplayNode* node)
{
    if (!node)
    {
        return;
    }
    // A node can only be registered in one set, otherwise ~DisplayNode would
    // deregister it from just one of them and leave the other dangling.
    if (node->selectionSet && node->selectionSet != this)
    {
        node->selectionSet->erase(node);
    }
    if (nodes.insert(node).second)
    {
        node->selectionSet = this;
    }
}

void SelectionSet::erase(DisplayNode* node)
{
    if (!node)
    {
        return;
    }
    if (nodes.erase(node) > 0)
    {
        node->selectionSet = nullptr;
    }
}

void SelectionSet::clear()
{
    for (DisplayNode* node : nodes)
    {
        node->selected = false;
        node->selectionSet = nullptr;
    }
    nodes.clear();
}

void clearSelectedChildren(DisplayNode* node, SelectionSet& selectedNodes)
{
    node->selected = false;
    selectedNodes.erase(node);
    for (auto& child : node->children)
    {
        clearSelectedChildren(child.get(), selectedNodes);
    }
}
