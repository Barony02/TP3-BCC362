#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <memory>
#include "NodeInfo.hpp"

struct StoreResponse {
    bool success;
    int version;
    std::string value;
};

class QuorumClient {
private:
    std::vector<NodeInfo> storeNodes;
    int N_W = 2; // Quórum de Escrita (N=3)
    int N_R = 2; // Quórum de Leitura (N=3)[cite: 1]

    StoreResponse sendToNode(const NodeInfo& node, const std::string& key, const std::string& value, int version, bool isWrite);

public:
    QuorumClient(const std::vector<NodeInfo>& nodes) : storeNodes(nodes) {}

    // Protocolo 3: Escrita por Quórum (Gifford)[cite: 1]
    bool quorumWrite(const std::string& key, const std::string& value);
    
    // Protocolo 3: Leitura por Quórum[cite: 1]
    std::string quorumRead(const std::string& key);
};