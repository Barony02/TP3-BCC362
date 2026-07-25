#include "RequestVoteMessageSerializer.hpp"
#include <cstring>

std::vector<char> RequestVoteMessageSerializer::serialize(RequestVoteMessage msg)
{
    std::vector<char> buffer;

    int id = msg.getcID();
    int term = msg.getcTerm();
    int logLength = msg.getcLogLength();
    int logTerm = msg.getcLogTerm();

    // A ordem de inserção aqui define a ordem dos bytes
    buffer.insert(buffer.end(), (char*)&id, (char*)&id + sizeof(id));
    buffer.insert(buffer.end(), (char*)&term, (char*)&term + sizeof(term));
    buffer.insert(buffer.end(), (char*)&logLength, (char*)&logLength + sizeof(logLength));
    buffer.insert(buffer.end(), (char*)&logTerm, (char*)&logTerm + sizeof(logTerm));

    return buffer;
}

RequestVoteMessage RequestVoteMessageSerializer::deserialize(const std::vector<char> buffer)
{
    const char* ptr = buffer.data();
    int id, term, logLength, logTerm;

    // A ordem de extração deve ser idêntica à de inserção
    memcpy(&id, ptr, sizeof(id));
    ptr += sizeof(id);

    memcpy(&term, ptr, sizeof(term));
    ptr += sizeof(term);

    memcpy(&logLength, ptr, sizeof(logLength));
    ptr += sizeof(logLength);

    memcpy(&logTerm, ptr, sizeof(logTerm));

    return RequestVoteMessage(id, term, logLength, logTerm);
}