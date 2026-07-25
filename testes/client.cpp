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
    std::string baseIp = "127.0.0.1";

    // Mapeia os 3 Nós do Cluster Sync (Raft)[cite: 1, 9]
    std::vector<NodeInfo> raftCluster = {
        NodeInfo(1, 8001, "10.128.0.8"),
        NodeInfo(2, 8002, "10.128.0.7"),
        NodeInfo(3, 8003, "10.128.0.9")
    };

    Network network;

    std::cout << "==========================================" << std::endl;
    std::cout << " 🖥️ CLIENTE RAFT COM DESTINO ALEATÓRIO" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Geradores aleatórios
    std::uniform_int_distribution<> nodeDist(0, raftCluster.size() - 1); // Sortear Nó Raft
    std::uniform_int_distribution<> resourceDist(0, 2);                  // Sortear Recurso A, B ou C[cite: 1]
    std::uniform_int_distribution<> reqDist(10, 20);                     // Total de requisições
    std::uniform_int_distribution<> timeDist(1, 3);                      // Intervalo entre envios

    std::vector<std::string> recursos = {"recurso_A", "recurso_B", "recurso_C"};
    
    int numRequests = reqDist(gen);
    int clientId = 999; 
    int clientPort = 9005; // Usando a porta 9005 para não conflitar com o Store[cite: 10]

    int listenfd = createListeningSocket(clientPort);
    if (listenfd < 0) {
        std::cerr << " [Erro Crítico] Falha ao abrir porta de escuta local " << clientPort << std::endl;
        return 1;
    }

    std::cout << "Iniciando bateria de " << numRequests << " requisições aleatórias...\n" << std::endl;

    for (int i = 1; i <= numRequests; ++i) {
        // 1. Sorteia um nó aleatório do Cluster Sync para enviar a requisição
        NodeInfo targetNode = raftCluster[nodeDist(gen)];

        // 2. Sorteia qual dos recursos será alterado
        std::string key = recursos[resourceDist(gen)];
        std::string value = "Novo_Valor_Req_" + std::to_string(i);

        ClientInfo clientInfo(clientPort, "10.128.0.10", clientId);
        ClientCommand cmd(clientInfo, Operation::WRITE, key, value);

        std::cout << "[Req " << i << "/" << numRequests << "] Enviando para Nó Raft " << targetNode.getid() 
                  << " (Porta " << targetNode.getport() << "): Modificar '" << key << "' -> '" << value << "'\n";
        
        sendClientCommandStruct sendCmd(targetNode, cmd);
        network.sendClientCommand(sendCmd);

        std::cout << "    -> Aguardando confirmação..." << std::endl;

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int responseSock = accept(listenfd, (sockaddr*)&clientAddr, &clientLen);

        if (responseSock >= 0) {
            auto msg = network.receiveMessage(responseSock);
            
            if (msg && msg->msgtype == messageType::SEND_CLIENT_RESPONSE) {
                auto response = static_cast<sendClientResponseStruct*>(msg.get());
                std::cout << "    -> Resposta do Cluster: " << response->status << std::endl;
            } else {
                std::cerr << "    -> Erro: Resposta inválida." << std::endl;
            }

            close(responseSock);
        } else {
            std::cerr << "    -> Erro ao aceitar conexão de resposta." << std::endl;
        }

        if (i < numRequests) {
            int delay = timeDist(gen);
            std::cout << "    -> Sucesso! Aguardando " << delay << "s...\n\n";
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }
    }

    close(listenfd);

    std::cout << "\n==========================================" << std::endl;
    std::cout << " Bateria de testes finalizada com sucesso." << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}