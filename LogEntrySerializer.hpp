#ifndef LOGENTRYSERIALIZER_HPP
#define LOGENTRYSERIALIZER_HPP

#include "LogEntry.h"
#include <cstring>
#include <vector>

class LogEntrySerializer{
public:
    static std::vector<char> serialize(LogEntry& entry);
    static LogEntry deserialize(std::vector<char>& buffer);
};

#endif