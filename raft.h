#ifndef RAFT_H
#define RAFT_H

#include <vector>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <mutex>   // <--- ADICIONADO para controle de concorrência
#include <thread>  // <--- ADICIONADO para gerenciar threads internas
#include <random>  // <--- ADICIONADO para aleatoriedade do timeout
#include "Role.h"
#include "Log.h"
#include "NodeInfo.hpp"
#include "RequestVoteMessage.hpp"
#include "Network.h"

class raft
{
private:
    NodeInfo node;
    int currentTerm;
    int votedFor;
    NodeInfo currentLeader;
    int commitLength;
    std::unordered_map<int, int> sentLength;
    std::unordered_map<int, int> ackedLength;
    std::unordered_set<int> votesReceived;
    std::chrono::steady_clock::time_point lastHeartBeat;
    std::chrono::milliseconds timeout;
    std::unordered_map<int, int> database;

    Log log;
    std::vector<NodeInfo> cluster;
    Network network;
    Role role;

    std::mutex stateMutex;        
    bool running;                  
    std::thread listenerThread;   
    std::thread tickerThread;      // Thread dedicada aos timeouts de eleição/heartbeats
    
    void tickerLoop();             // Loop do temporizador (Background)
    void sendHeartbeats();         // Envia heartbeats vazios se for o líder
    void resetElectionTimeout();   // Define um timeout aleatório para evitar colisões

public:
    raft(int id, int port, std::string addr);
    ~raft();                       // Destrutor para parar as threads com segurança

    void start();                  // Dispara o nó Raft de forma autônoma
    void stop();                   // Para o nó e fecha as threads
    void addClusterMember(const NodeInfo& member); // Configura os nós vizinhos

    void recoverFromCrash();
    void leaderCrashed();
    void newElection();
    void collectVotes(int VoterID, int Term, bool granted);
    void broadcastElectionMessages(RequestVoteMessage msg, NodeInfo node);
    void receiveElectionMessage(RequestVoteMessage msg, NodeInfo candidate);

    void replicateLog(NodeInfo follower);
    void followerReceiveAppendEntries(NodeInfo Leader, int term, int prefixLen, int prefixTerm, int leaderCommit, std::vector<LogEntry> suffix);
    void followerAppend(int prefixLen, int leaderCommit, std::vector<LogEntry> suffix);
    
    void broadcastClientMessage(ClientCommand msg);
    void logAcknowledgment(int followerID, int term, int ack, bool success);
    void commitLog();

    // Método centralizador de processamento que criamos no passo anterior
    void processMessage(std::unique_ptr<messageBase> msg); 
    Network& getNetwork(){return network;}
    void applyLogToStateMachine(LogEntry entry);
};

#endif