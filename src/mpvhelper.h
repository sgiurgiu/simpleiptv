#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include <mpv/client.h>

struct Wrapper;
using NodeVariantMap = std::map<std::string, Wrapper>;
using NodeVariantList = std::vector<Wrapper>;
using NodeVariant =
    std::variant<std::string, bool, int64_t, int, double, NodeVariantMap, NodeVariantList>;
// using NodeVariant = std::variant<std::string, bool, int64_t, int, double>;
struct Wrapper
{
    NodeVariant node;
};

struct BuilderVisitor
{
    mpv_node_list* createList(const NodeVariantList& list) const
    {
        mpv_node_list* l = new mpv_node_list;
        l->num = static_cast<int>(list.size());
        l->values = new mpv_node[l->num];
        int i = 0;
        for (const auto& v : list)
        {
            l->values[i] = std::visit(*this, v.node);
            ++i;
        }
        return l;
    }
    mpv_node_list* createMap(const NodeVariantMap& map) const
    {
        mpv_node_list* l = new mpv_node_list;
        l->num = static_cast<int>(map.size());
        l->values = new mpv_node[l->num];
        l->keys = new char*[l->num];
        int i = 0;
        for (const auto& v : map)
        {
            l->keys[i] = new char[v.first.size() + 1];
            memcpy(l->keys[i], v.first.c_str(), v.first.size() + 1);
            l->keys[i][v.first.size()] = 0;
            l->values[i] = std::visit(*this, v.second.node);
            ++i;
        }
        return l;
    }
    mpv_node operator()(bool b) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_FLAG;
        node.u.flag = b;
        return node;
    }
    mpv_node operator()(const std::string& s) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_STRING;
        node.u.string = new char[s.size() + 1];
        memcpy(node.u.string, s.c_str(), s.size() + 1);
        node.u.string[s.size()] = 0;
        return node;
    }
    mpv_node operator()(int64_t i) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_INT64;
        node.u.int64 = i;
        return node;
    }
    mpv_node operator()(int i) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_INT64;
        node.u.int64 = i;
        return node;
    }
    mpv_node operator()(double d) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_DOUBLE;
        node.u.double_ = d;
        return node;
    }
    mpv_node operator()(const NodeVariantMap& map) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_NODE_MAP;
        node.u.list = createMap(map);
        return node;
    }
    mpv_node operator()(const NodeVariantList& list) const
    {
        mpv_node node;
        node.format = MPV_FORMAT_NODE_ARRAY;
        node.u.list = createList(list);
        return node;
    }
};

class NodeBuilder
{
public:
    NodeBuilder(const NodeVariant& nodeVariant)
    {
        node = std::visit(BuilderVisitor{}, nodeVariant);
    }
    ~NodeBuilder()
    {
        releaseNode(&node);
    }

    mpv_node* GetNode()
    {
        return &node;
    }

private:
    void releaseNode(mpv_node* node)
    {
        switch (node->format)
        {
        case MPV_FORMAT_STRING:
            delete[] node->u.string;
            break;
        case MPV_FORMAT_NODE_ARRAY:
            if (node->u.list)
            {
                for (int i = 0; i < node->u.list->num; i++)
                {
                    releaseNode(&node->u.list->values[i]);
                }
                delete[] node->u.list->values;
                delete node->u.list;
            }
            break;
        case MPV_FORMAT_NODE_MAP:
            if (node->u.list)
            {
                for (int i = 0; i < node->u.list->num; i++)
                {
                    releaseNode(&node->u.list->values[i]);
                    delete[] node->u.list->keys[i];
                }
                delete[] node->u.list->values;
                delete[] node->u.list->keys;
                delete node->u.list;
            }
            break;
        default:
            break;
        }
        node->format = MPV_FORMAT_NONE;
    }
    mpv_node node;
};