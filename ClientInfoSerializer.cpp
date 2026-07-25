#include "ClientInfoSerializer.h"
#include <cstring>

std::vector<char> ClientInfoSerializer::serialize(ClientInfo client)
{
    std::vector<char> buffer;

    int id = client.id;
    int port = client.port;
    std::string address = client.ip;
    int addrLen = address.size();

    // Insere os inteiros (4 bytes cada)
    buffer.insert(buffer.end(), (char*)&id, (char*)&id + sizeof(id));
    buffer.insert(buffer.end(), (char*)&port, (char*)&port + sizeof(port));
    
    // Insere o tamanho da string de endereço, seguido pelos caracteres da string
    buffer.insert(buffer.end(), (char*)&addrLen, (char*)&addrLen + sizeof(addrLen));
    buffer.insert(buffer.end(), address.begin(), address.end());

    return buffer;
}

ClientInfo ClientInfoSerializer::deserialize(const std::vector<char>& buffer)
{
    const char* ptr = buffer.data();

    int id, port, addrLen;

    // Extrai o ID
    std::memcpy(&id, ptr, sizeof(id));
    ptr += sizeof(id);

    // Extrai a porta
    std::memcpy(&port, ptr, sizeof(port));
    ptr += sizeof(port);

    // Extrai o tamanho da string do IP
    std::memcpy(&addrLen, ptr, sizeof(addrLen));
    ptr += sizeof(addrLen);

    // Extrai a string do IP usando o tamanho lido
    std::string address(ptr, addrLen);

    // Reconstrói e retorna o objeto ClientInfo
    return ClientInfo(port, address, id);
} 
