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
    int clientId = (argc > 1) ? std::stoi(argv[1]) : 999;
    int clientPort = (argc > 2) ? std::stoi(argv[2]) : 9005;

    std::vector<NodeInfo> raftCluster = {
        NodeInfo(1, 8001, "136.65.16.176"),
        NodeInfo(2, 8002, "34.173.86.220"),
        NodeInfo(3, 8003, "34.121.142.211"),
        NodeInfo(4, 8004, "35.185.108.20"),
        NodeInfo(5, 8005, "35.196.220.244")
    };

    Network network;

    std::cout << "==========================================" << std::endl;
    std::cout << " 🖥️ CLIENTE " << clientId << " (Porta " << clientPort << ") ONLINE" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd() ^ clientId);
    
    std::uniform_int_distribution<> resourceDist(0, 2);
    std::uniform_int_distribution<> reqDist(15, 25);

    std::vector<std::string> recursos = {"recurso_A", "recurso_B", "recurso_C"};
    
    int numRequests = reqDist(gen);

    int listenfd = createListeningSocket(clientPort);
    if (listenfd < 0) {
        std::cerr << " [Erro Crítico] Falha ao abrir porta de escuta local " << clientPort << std::endl;
        return 1;
    }

    // Início alternado com base no ID do cliente
    int currentTargetIdx = clientId % raftCluster.size();

    std::cout << "Iniciando bateria de " << numRequests << " requisições...\n" << std::endl;

    for (int i = 1; i <= numRequests; ++i) {
        std::string key = recursos[resourceDist(gen)];
        std::string value = "Val_C" + std::to_string(clientId) + "_Req" + std::to_string(i);

        ClientInfo clientInfo(clientPort, "34.70.245.113", clientId);
        ClientCommand cmd(clientInfo, Operation::WRITE, key, value);

        bool sucesso = false;
        int tentativas = 0;
        int totalNodes = raftCluster.size();

        while (!sucesso && tentativas < totalNodes * 2) {
            NodeInfo targetNode = raftCluster[currentTargetIdx];
            tentativas++;

            std::cout << "[Cliente " << clientId << " | Req " << i << "/" << numRequests 
                      << "] -> Envia para Nó " << targetNode.getid() << "... " << std::flush;

            sendClientCommandStruct sendCmd(targetNode, cmd);

            try {
                network.sendClientCommand(sendCmd);
            } catch (...) {
                std::cout << "❌ [FALHA DE REDE]\n";
                currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
                continue;
            }

            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int responseSock = accept(listenfd, (sockaddr*)&clientAddr, &clientLen);

            if (responseSock >= 0) {
                auto msg = network.receiveMessage(responseSock);
                
                if (msg && msg->msgtype == messageType::SEND_CLIENT_RESPONSE) {
                    auto response = static_cast<sendClientResponseStruct*>(msg.get());
                    std::cout << "✅ [OK] Resposta: " << response->status << std::endl;
                    sucesso = true;
                    // Alterna o nó alvo para a próxima requisição
                    currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
                } else {
                    std::cout << "⚠️ [Resposta Inválida]. Alternando nó...\n";
                    currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
                }
                close(responseSock);
            } else {
                std::cout << "⏱️ [TIMEOUT]. Alternando nó...\n";
                currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
            }
        }

        if (!sucesso) {
            std::cerr << "❌ [ERRO CRÍTICO] Nenhum nó respondeu para a Req " << i << "\n";
            // Garante rotação mesmo se falhar todas as tentativas
            currentTargetIdx = (currentTargetIdx + 1) % totalNodes;
        }
    }

    close(listenfd);

    std::cout << "\n==========================================" << std::endl;
    std::cout << " Cliente " << clientId << " finalizou todas as requisições." << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}