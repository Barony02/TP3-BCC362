#include "../raft.h"
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    // Validação de segurança robusta
    if (argc < 4) {
        std::cerr << "==========================================================" << std::endl;
        std::cerr << " ERRO DE INICIALIZAÇÃO DO NÓ" << std::endl;
        std::cerr << " Uso: ./raft_cluster [MEU_ID] [MINHA_PORTA] [TOTAL_DE_NOS]" << std::endl;
        std::cerr << " Exemplo para um cluster de 3 nós:" << std::endl;
        std::cerr << " Terminal 1: ./raft_cluster 1 8001 3" << std::endl;
        std::cerr << " Terminal 2: ./raft_cluster 2 8002 3" << std::endl;
        std::cerr << " Terminal 3: ./raft_cluster 3 8003 3" << std::endl;
        std::cerr << "==========================================================" << std::endl;
        return 1;
    }

    int myId = std::stoi(argv[1]);
    int myPort = std::stoi(argv[2]);
    int numNodes = std::stoi(argv[3]);

    // Instancia o nó local na interface local
    raft localNode(myId, myPort, "127.0.0.1");

    // Popula o Cluster Dinamicamente com base no parâmetro
    int basePort = 8000;
    for (int i = 1; i <= numNodes; i++) {
        if (i != myId) {
            // Assume sequencialidade para o teste (ex: id 1 = porta 8001, id 2 = 8002)
            localNode.addClusterMember(NodeInfo(i, basePort + i, "127.0.0.1"));
        }
    }

    // Dispara a escuta de rede e a máquina de estados em threads de background
    localNode.start();

    std::cout << "\n==========================================" << std::endl;
    std::cout << " 🟢 NÓ " << myId << " ONLINE (Porta: " << myPort << ")" << std::endl;
    std::cout << " Tamanho do Cluster configurado: " << numNodes << " nós." << std::endl;
    std::cout << " Aguardando estabilização da rede..." << std::endl;
    std::cout << " [Pressione ENTER a qualquer momento para matar o nó]" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    // Trava a Thread Principal (Evita o fim do programa) simulando o servidor
    std::cin.get(); 

    std::cout << "🛑 Encerrando Nó " << myId << " graciosamente..." << std::endl;
    localNode.stop();
    
    return 0;
}