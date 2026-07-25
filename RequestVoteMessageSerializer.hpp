#ifndef REQUESTVOTEMESSAGESERIALIZER_H
#define REQUESTVOTEMESSAGESERIALIZER_H

#include "RequestVoteMessage.hpp"
#include <vector>

class RequestVoteMessageSerializer{
public:
    static std::vector<char> serialize(RequestVoteMessage msg);
    static RequestVoteMessage deserialize(const std::vector<char> buffer);
};

#endif