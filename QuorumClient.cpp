#include "QuorumClient.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

StoreResponse QuorumClient::sendToNode(const NodeInfo& node, const std::string& key, const std::string& value, int version, bool isWrite) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {false, -1, ""};

    // Timeout de 1 segundo para suportar simulação de falha por queda/omissão do enunciado[cite: 1]
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(node.getport());
    inet_pton(AF_INET, node.getaddress().c_str(), &server.sin_addr);

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        close(sock);
        return {false, -1, ""};
    }

    char type = isWrite ? 'W' : 'R';
    send(sock, &type, 1, 0);

    int keySize = key.size();
    send(sock, &keySize, sizeof(keySize), 0);
    send(sock, key.c_str(), keySize, 0);

    if (isWrite) {
        int valSize = value.size();
        send(sock, &valSize, sizeof(valSize), 0);
        send(sock, value.c_str(), valSize, 0);
        send(sock, &version, sizeof(version), 0);
    }

    bool ok = false;
    int respVersion = -1;
    int respValSize = 0;
    std::string respValue = "";

    if (recv(sock, &ok, sizeof(ok), 0) > 0 && ok) {
        recv(sock, &respVersion, sizeof(respVersion), 0);
        if (!isWrite) {
            recv(sock, &respValSize, sizeof(respValSize), 0);
            if (respValSize > 0) {
                respValue.resize(respValSize);
                recv(sock, &respValue[0], respValSize, 0);
            }
        }
    }

    close(sock);
    return {ok, respVersion, respValue};
}

bool QuorumClient::quorumWrite(const std::string& key, const std::string& value) {
    int maxVersion = -1;
    int readAcks = 0;

    // Etapa 1: Descobrir o número de versão mais recente nas réplicas do Store (Gifford)[cite: 1]
    for (const auto& node : storeNodes) {
        auto res = sendToNode(node, key, "", 0, false);
        if (res.success) {
            readAcks++;
            maxVersion = std::max(maxVersion, res.version);
        }
    }

    if (readAcks < N_R) {
        std::cerr << "[QuorumWrite] Falha: Quórum de leitura insuficiente para determinar versão." << std::endl;
        return false;
    }

    int newVersion = maxVersion + 1; // Incrementa a versão mais recente encontrada[cite: 1]
    int writeAcks = 0;

    // Etapa 2: Atualizar a nova versão no quórum Nw[cite: 1]
    for (const auto& node : storeNodes) {
        auto res = sendToNode(node, key, value, newVersion, true);
        if (res.success) {
            writeAcks++;
        }
    }

    // Retorna verdadeiro se atingiu o quórum de escrita[cite: 1]
    return writeAcks >= N_W;
}