#pragma once

#include "display_node.h"

void clearSelectedChildren(DisplayNode* node,
                           std::unordered_set<DisplayNode*>& selectedNodes);