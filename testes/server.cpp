#include "../raft.h"
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "==========================================================" << std::endl;
        std::cerr << " ERRO DE INICIALIZAÇÃO DO NÓ" << std::endl;
        std::cerr << " Uso: ./raft_cluster [MEU_ID] [MEU_IP_INTERNO]" << std::endl;
        std::cerr << " Exemplo:" << std::endl;
        std::cerr << "  VM 1: ./raft_cluster 1 10.128.0.8" << std::endl;
        std::cerr << "  VM 2: ./raft_cluster 2 10.128.0.7" << std::endl;
        std::cerr << "  VM 3: ./raft_cluster 3 10.128.0.9" << std::endl;
        std::cerr << "==========================================================" << std::endl;
        return 1;
    }

    int myId = std::stoi(argv[1]);
    std::string myIp = argv[2];

    // Mapeamento dos nós do Cluster conforme configurado no client.cpp
    std::vector<NodeInfo> allNodes = {
        NodeInfo(1, 8001, "10.128.0.8"),
        NodeInfo(2, 8002, "10.128.0.7"),
        NodeInfo(3, 8003, "10.128.0.9")
    };

    // Descobre qual é a minha porta baseada no ID
    int myPort = 8000 + myId;
    for (const auto& node : allNodes) {
        if (node.getid() == myId) {
            myPort = node.getport();
            break;
        }
    }

    // Instancia o nó local com o IP real da GCP
    raft localNode(myId, myPort, myIp);

    // Popula o Cluster com os IPs reais dos outros nós
    for (const auto& member : allNodes) {
        if (member.getid() != myId) {
            localNode.addClusterMember(member);
        }
    }

    // Dispara a escuta de rede e a máquina de estados em threads
    localNode.start();

    std::cout << "\n==========================================" << std::endl;
    std::cout << " 🟢 NÓ " << myId << " ONLINE (IP: " << myIp << " | Porta: " << myPort << ")" << std::endl;
    std::cout << " Cluster configurado com 3 nós GCP." << std::endl;
    std::cout << " Aguardando estabilização da rede..." << std::endl;
    std::cout << " [Pressione ENTER a qualquer momento para matar o nó]" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    std::cin.get(); 

    std::cout << "🛑 Encerrando Nó " << myId << " graciosamente..." << std::endl;
    localNode.stop();
    
    return 0;
}