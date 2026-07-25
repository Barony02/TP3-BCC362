// store_node.cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>

struct Item {
    std::string value;
    int version = 0; //
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: ./store_node [PORTA]" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::unordered_map<std::string, Item> database;

    // Inicializa os 3 recursos fixos no banco local
    database["recurso_A"] = {"Texto Inicial A", 0};
    database["recurso_B"] = {"Texto Inicial B", 0};
    database["recurso_C"] = {"Texto Inicial C", 0};

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listenfd, (sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 10);

    std::cout << "==========================================" << std::endl;
    std::cout << " 🟢 CLUSTER STORE " << port << " ONLINE" << std::endl;
    std::cout << " Recursos disponíveis: recurso_A, recurso_B, recurso_C" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    while (true) {
        int conn = accept(listenfd, nullptr, nullptr);
        if (conn < 0) continue;

        char type;
        if (recv(conn, &type, 1, 0) <= 0) { close(conn); continue; }

        int keySize;
        recv(conn, &keySize, sizeof(keySize), 0);
        std::string key(keySize, 0);
        recv(conn, &key[0], keySize, 0);

        if (type == 'R') { // Leitura de Versão/Dado
            Item item = database[key];
            bool ok = true;
            
            send(conn, &ok, sizeof(ok), 0);
            send(conn, &item.version, sizeof(item.version), 0);
            
            int valSize = item.value.size();
            send(conn, &valSize, sizeof(valSize), 0);
            if (valSize > 0) {
                send(conn, item.value.c_str(), valSize, 0);
            }

            std::cout << "[Store " << port << " | READ] Chave: '" << key 
                      << "' -> Versão Atual: " << item.version 
                      << " | Conteúdo: \"" << item.value << "\"" << std::endl;
        } 
        else if (type == 'W') { // Escrita com Validação de Quórum[cite: 1]
            int valSize, newVersion;
            recv(conn, &valSize, sizeof(valSize), 0);
            std::string value(valSize, 0);
            recv(conn, &value[0], valSize, 0);
            recv(conn, &newVersion, sizeof(newVersion), 0);

            // Verifica a regra de atualização do número de versão[cite: 1]
            if (newVersion >= database[key].version) {
                int oldVersion = database[key].version;
                database[key] = {value, newVersion};
                
                bool ok = true;
                send(conn, &ok, sizeof(ok), 0);
                send(conn, &newVersion, sizeof(newVersion), 0);

                std::cout << "--------------------------------------------------" << std::endl;
                std::cout << " 🟢 [Store " << port << " | WRITE ACEITA]" << std::endl;
                std::cout << " Chave:      '" << key << "'" << std::endl;
                std::cout << " Transição:  Versão " << oldVersion << " -> Versão " << newVersion << std::endl;
                std::cout << " Novo Valor: \"" << value << "\"" << std::endl;
                std::cout << "--------------------------------------------------" << std::endl;
            } else {
                bool ok = false;
                send(conn, &ok, sizeof(ok), 0);
                send(conn, &database[key].version, sizeof(database[key].version), 0);

                std::cout << " 🔴 [Store " << port << " | WRITE REJEITADA]" << std::endl;
                std::cout << " Motivo: Versão recebida (" << newVersion 
                          << ") é menor que a versão atual (" << database[key].version << ")" << std::endl;
            }
        }

        close(conn);
    }

    close(listenfd);
    return 0;
}