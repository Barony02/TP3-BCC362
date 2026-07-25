#ifndef NODEINFOSERIALIZER_H
#define NODEINFOSERIALIZER_H

#include "NodeInfo.hpp"
#include <vector>

class NodeInfoSerializer {
public:
    static std::vector<char> serialize(NodeInfo& node);
    static NodeInfo deserialize(const std::vector<char>& buffer);
};

#endif