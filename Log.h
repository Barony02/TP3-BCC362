#ifndef LOG_H
#define LOG_H
#include "LogEntry.h"
#include <vector>
class Log
{
    std::vector<LogEntry> logentries;

public:
    Log() { logentries = {}; }
    Log(std::vector<LogEntry> entries)
    {
        for (auto entry : entries)
            logentries.push_back(entry);
    }
    std::vector<LogEntry>& getEntries() { return logentries; }
    void append(LogEntry log) { logentries.push_back(log); }
    void truncate(size_t index){
        if (index < logentries.size())
            logentries.erase(logentries.begin() + index, logentries.end());
    }
};
#endif
