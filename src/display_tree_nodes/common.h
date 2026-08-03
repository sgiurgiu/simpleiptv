#pragma once

#include <unordered_set>
struct DisplayNode;

/**
 * The set of currently selected nodes of one tree (local channels, remote
 * servers, ...).
 *
 * It holds raw DisplayNode pointers, but a node deregisters itself from the set
 * it belongs to in ~DisplayNode. Nodes are destroyed behind the selection's
 * back all the time - a channels reload throws the whole tree away, "Remove
 * Favourite" erases a single node, a server refresh replaces a subtree - and
 * without that deregistration the set keeps dangling pointers that the next
 * clear() writes `selected = false` through.
 */
class SelectionSet
{
public:
    SelectionSet() = default;
    ~SelectionSet();
    SelectionSet(const SelectionSet&) = delete;
    SelectionSet& operator=(const SelectionSet&) = delete;

    void insert(DisplayNode* node);
    void erase(DisplayNode* node);
    /// Deselects every node still in the set, then empties it.
    void clear();
    bool empty() const
    {
        return nodes.empty();
    }

private:
    std::unordered_set<DisplayNode*> nodes;
};

void clearSelectedChildren(DisplayNode* node, SelectionSet& selectedNodes);
