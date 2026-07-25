#include "ClientCommandSerializer.h"
#include <cstring>
#include <stdexcept>

std::vector<char> ClientCommandSerializer::serialize(const ClientCommand& cmd)
{
    std::vector<char> buffer;

    auto append = [&](const void* data, size_t size)
    {
        const char* ptr = static_cast<const char*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    };

    // CORREÇÃO: Serializa os 3 campos essenciais de ClientInfo: ID, Porta e IP[cite: 9, 10]
    int clientID = cmd.getClient().id;
    append(&clientID, sizeof(clientID));

    int clientPort = cmd.getClient().port;
    append(&clientPort, sizeof(clientPort));

    std::string clientIp = cmd.getClient().ip;
    int ipSize = clientIp.size();
    append(&ipSize, sizeof(ipSize));
    append(clientIp.data(), ipSize);

    // Serializa o restante do comando original[cite: 10]
    int operation = static_cast<int>(cmd.getOperation());
    append(&operation, sizeof(operation));

    int keySize = cmd.getKey().size();
    append(&keySize, sizeof(keySize));
    append(cmd.getKey().data(), keySize);

    int valueSize = cmd.getValue().size();
    append(&valueSize, sizeof(valueSize));
    append(cmd.getValue().data(), valueSize);

    return buffer;
}

ClientCommand ClientCommandSerializer::deserialize(const std::vector<char>& buffer)
{
    size_t offset = 0;

    auto read = [&](void* dest, size_t size)
    {
        if (offset + size > buffer.size()) {
            throw std::runtime_error("Erro de desserialização: buffer menor que o esperado.");
        }
        std::memcpy(dest, buffer.data() + offset, size);
        offset += size;
    };

    // CORREÇÃO: Desserializa na exata mesma ordem: ID, Porta e IP[cite: 9, 10]
    int clientID;
    read(&clientID, sizeof(clientID));

    int clientPort;
    read(&clientPort, sizeof(clientPort));

    int ipSize;
    read(&ipSize, sizeof(ipSize));

    std::string clientIp;
    clientIp.resize(ipSize);
    read(&clientIp[0], ipSize);

    int operation;
    read(&operation, sizeof(operation));

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

    // CORREÇÃO: Reconstrói o ClientInfo com os dados reais de IP e Porta que vieram da rede[cite: 10]
    ClientInfo reconstructedClient(clientPort, clientIp, clientID);

    return ClientCommand(
        reconstructedClient,
        static_cast<Operation>(operation),
        key,
        value
    );
}