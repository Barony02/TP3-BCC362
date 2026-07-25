#ifndef CLIENTINFOSERIALIZER_H
#define CLIENTINFOSERIALIZER_H

#include "ClientInfo.h" // Certifique-se de que o nome do seu header está correto
#include <vector>

class ClientInfoSerializer {
public:
    static std::vector<char> serialize(ClientInfo client);
    static ClientInfo deserialize(const std::vector<char>& buffer);
};

#endif 
