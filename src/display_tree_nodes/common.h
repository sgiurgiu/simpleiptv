#pragma once

#include <unordered_set>
struct DisplayNode;
void clearSelectedChildren(DisplayNode* node,
                           std::unordered_set<DisplayNode*>& selectedNodes);