// testes/client.cpp
#include "../Network.h"
#include "../LogEntry.h"
#include "../NodeInfo.hpp"
#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int createListeningSocket(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) return -1;

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // TIMEOUT NO ACCEPT: Se o nó selecionado caiu ou não respondeu, 
    // libera o accept em 2 segundos para tentar o próximo nó do cluster.
    struct timeval tv;
    tv.tv_sec = 2; 
    tv.tv_usec = 0;
    setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenfd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(listenfd);
        return -1;
    }

    if (listen(listenfd, 5) < 0) {
        close(listenfd);
        return -1;
    }

    return listenfd;
}

int main(int argc, char* argv[]) {
    // Mapeia os 5 Nós do Cluster Sync (Raft)
    std::vector<NodeInfo> raftCluster = {
        NodeInfo(1, 8001, "136.65.16.176"),
        NodeInfo(2, 8002, "34.173.86.220"),
        NodeInfo(3, 8003, "34.121.142.211"),
        NodeInfo(4, 8004, "35.185.108.20"),
        NodeInfo(5, 8005, "35.196.220.244")
    };

    Network network;

    std::cout << "==========================================" << std::endl;
    std::cout << " 🖥️ CLIENTE RAFT COM FAILOVER AUTOMÁTICO" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::uniform_int_distribution<> resourceDist(0, 2);                  // Sortear Recurso A, B ou C[cite: 1]
    std::uniform_int_distribution<> reqDist(10, 20);                     // Total de requisições
    std::uniform_int_distribution<> timeDist(1, 3);                      // Intervalo entre envios

    std::vector<std::string> recursos = {"recurso_A", "recurso_B", "recurso_C"};
    
    int numRequests = reqDist(gen);
    int clientId = 999; 
    int clientPort = 9005; // Porta de escuta local do cliente

    int listenfd = createListeningSocket(clientPort);
    if (listenfd < 0) {
        std::cerr << " [Erro Crítico] Falha ao abrir porta de escuta local " << clientPort << std::endl;
        return 1;
    }

    // O cliente começa tentando o primeiro nó (índice 0)
    int currentTargetIdx = 0;

    std::cout << "Iniciando bateria de " << numRequests << " requisições...\n" << std::endl;

    for (int i = 1; i <= numRequests; ++i) {
        std::string key = recursos[resourceDist(gen)];
        std::string value = "Novo_Valor_Req_" + std::to_string(i);

        ClientInfo clientInfo(clientPort, "34.70.245.113", clientId);
        ClientCommand cmd(clientInfo, Operation::WRITE, key, value);

        bool sucesso = false;
        int tentativas = 0;
        int totalNodes = raftCluster.size();

        // Tenta enviar a requisição rodando pelos nós do cluster até obter uma resposta válida
        // Em testes/client.cpp

        while (!sucesso && tentativas < totalNodes * 2) {
            NodeInfo targetNode = raftCluster[currentTargetIdx];
            tentativas++;

            std::cout << "[Req " << i << "/" << numRequests << "] Enviando para Nó " 
                    << targetNode.getid() << " (" << targetNode.getaddress() << ":" << targetNode.getport() << ")... " << std::flush;

            sendClientCommandStruct sendCmd(targetNode, cmd);

            // 1. TENTA ENVIAR O COMANDO
            network.sendClientCommand(sendCmd);

            // 2. AGUARDA RESPOSTA COM TIMEOUT NO ACCEPT (2 segundos)
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int responseSock = accept(listenfd, (sockaddr*)&clientAddr, &clientLen);

            if (responseSock >= 0) {
                auto msg = network.receiveMessage(responseSock);
                
                if (msg && msg->msgtype == messageType::SEND_CLIENT_RESPONSE) {
                    auto response = static_cast<sendClientResponseStruct*>(msg.get());
                    std::cout << "\n    -> ✅ Resposta do Cluster: " << response->status << std::endl;
                    sucesso = true;
                } else {
                    std::cout << "\n    -> ⚠️ Resposta inválida/nó inalcançável. Tentando próximo...\n";
                    currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
                }
                close(responseSock);
            } else {
                // Se deu Timeout (errno == EAGAIN / EWOULDBLOCK), o nó caiu/ignorou. Alterna o nó alvo.
                std::cout << "⏱️ [TIMEOUT/SEM RESPOSTA] Nó " << targetNode.getid() << " não respondeu. Alternando nó alvo...\n";
                currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
            }
        }

        if (!sucesso) {
            std::cerr << "❌ [ERRO CRÍTICO] Nenhum nó do cluster respondeu para a Req " << i << "\n";
        }
    }

    close(listenfd);

    std::cout << "\n==========================================" << std::endl;
    std::cout << " Bateria de testes finalizada com sucesso." << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}