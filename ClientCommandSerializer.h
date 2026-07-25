#ifndef CLIENT_COMMAND_SERIALIZER_H
#define CLIENT_COMMAND_SERIALIZER_H

#include <vector>
#include "LogEntry.h"

class ClientCommandSerializer
{
public:
    static std::vector<char> serialize(const ClientCommand& cmd);
    static ClientCommand deserialize(const std::vector<char>& buffer);
};

#endif