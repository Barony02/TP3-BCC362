#include "NodeInfoSerializer.hpp"
#include <cstring>

std::vector<char> NodeInfoSerializer::serialize(NodeInfo& node)
{
    std::vector<char> buffer;

    int id = node.getid();
    int port = node.getport();
    std::string address = node.getaddress();

    int addressSize = address.size();

    buffer.insert(buffer.end(), (char*)&id, (char*)&id + sizeof(id));
    buffer.insert(buffer.end(), (char*)&port, (char*)&port + sizeof(port));
    buffer.insert(buffer.end(), (char*)&addressSize, (char*)&addressSize + sizeof(addressSize));
    buffer.insert(buffer.end(), address.begin(), address.end());

    return buffer;
}

NodeInfo NodeInfoSerializer::deserialize(const std::vector<char>& buffer)
{
    const char* ptr = buffer.data();

    int id;
    memcpy(&id, ptr, sizeof(id));
    ptr += sizeof(id);

    int port;
    memcpy(&port, ptr, sizeof(port));
    ptr += sizeof(port);

    int addressSize;
    memcpy(&addressSize, ptr, sizeof(addressSize));
    ptr += sizeof(addressSize);

    std::string address(ptr, addressSize);

    return NodeInfo(id, port, address);
}