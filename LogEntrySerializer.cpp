#include "LogEntrySerializer.hpp"
#include <cstring>
#include <stdexcept>

std::vector<char> LogEntrySerializer::serialize(LogEntry& entry)
{
    std::vector<char> buffer;

    auto append = [&](const void* data, size_t size)
    {
        const char* ptr = static_cast<const char*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    };

    int term = entry.getTerm();
    append(&term, sizeof(term));

    ClientCommand cmd = entry.getOperation();

    // CORREÇÃO 1: Acessa o ID do cliente através de cmd.getClient().id
    int clientID = cmd.getClient().id;
    append(&clientID, sizeof(clientID));

    int op = static_cast<int>(cmd.getOperation());
    append(&op, sizeof(op));

    int keySize = cmd.getKey().size();
    append(&keySize, sizeof(keySize));
    append(cmd.getKey().data(), keySize);

    int valueSize = cmd.getValue().size();
    append(&valueSize, sizeof(valueSize));
    append(cmd.getValue().data(), valueSize);

    return buffer;
}

LogEntry LogEntrySerializer::deserialize(std::vector<char>& buffer)
{
    size_t offset = 0;

    auto read = [&](void* dest, size_t size)
    {
        if (offset + size > buffer.size()) {
            throw std::runtime_error("Erro de desserialização em LogEntry: buffer menor que o esperado.");
        }
        std::memcpy(dest, buffer.data() + offset, size);
        offset += size;
    };

    int term;
    read(&term, sizeof(term));

    int clientID;
    read(&clientID, sizeof(clientID));

    int opInt;
    read(&opInt, sizeof(opInt));
    Operation op = static_cast<Operation>(opInt);

    int keySize;
    read(&keySize, sizeof(keySize));

    std::string key;
    key.resize(keySize);
    read(&key[0], keySize);

    int valueSize;
    read(&valueSize, sizeof(valueSize));

    std::string value;
    value.resize(valueSize);
    read(&value[0], valueSize);

    // CORREÇÃO 2: Cria a estrutura ClientInfo antes de instanciar o ClientCommand
    ClientInfo reconstructedClient(0, "", clientID);
    ClientCommand cmd(reconstructedClient, op, key, value);

    return LogEntry(cmd, term);
}