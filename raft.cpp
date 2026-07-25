#include "raft.h"
#include "QuorumClient.hpp"
#include <iostream>
#include <iomanip>
// raft.cpp

// O seu construtor deve estar assim:
raft::raft(int id, int port, std::string addr) 
    : node(id, port, addr), currentLeader(-1, -1, ""), running(false)
{
    currentTerm = 0;
    votedFor = -1;
    commitLength = 0;
    sentLength = {};
    ackedLength = {};
    votesReceived = {};
    role = Role::FOLLOWER;
    
    lastHeartBeat = std::chrono::steady_clock::now();
    resetElectionTimeout();
}

// 👉 O DESTRUTOR QUE ESTÁ FALTANDO:
raft::~raft() {
    stop();
}

void raft::recoverFromCrash()
{ // falta colocar um random de timeout nessa funcao aqui
    role = Role::FOLLOWER;
    votesReceived = {};
    sentLength = {};
    ackedLength = {};
}
void raft::leaderCrashed()
{
    // 1. Incrementa o Termo e muda para Candidato
    currentTerm += 1; 
    role = Role::CANDIDATE;
    
    // 2. Vota em si mesmo
    votedFor = node.getid();
    votesReceived.clear();
    votesReceived.insert(node.getid());
    
    // 3. Reseta o timer para não tentar uma nova eleição imediatamente
    resetElectionTimeout(); 

    // Agora cria a mensagem com o NOVO Termo
    int lastTerm = 0;
    if (log.getEntries().size() > 0)
        lastTerm = log.getEntries().back().getTerm();

    RequestVoteMessage Voterequest(node.getid(), currentTerm, log.getEntries().size(), lastTerm);

    for (auto targetNode : cluster)
    {
        if (targetNode.getid() != node.getid()) // Não envia para si mesmo
            network.sendRequestVote(sendRequestVoteStruct(targetNode, node, Voterequest));
    }
}
void raft::receiveElectionMessage(RequestVoteMessage msg, NodeInfo candidate)
{
    std::cout << "[Nó " << node.getid() << "] Recebi RequestVote do Nó " 
              << msg.getcID() << " (Termo: " << msg.getcTerm() << ")" << std::endl;

    if (msg.getcTerm() > currentTerm)
    {
        currentTerm = msg.getcTerm();
        role = Role::FOLLOWER;
        votedFor = -1;
    }

    int lastTerm = 0;
    if (log.getEntries().size() > 0)
        lastTerm = log.getEntries()[log.getEntries().size() - 1].getTerm();

    bool logOK = (msg.getcLogTerm() > lastTerm) || 
                 (msg.getcLogTerm() == lastTerm && static_cast<size_t>(msg.getcLogLength()) >= log.getEntries().size());

    if (msg.getcTerm() == currentTerm && logOK && (votedFor == -1 || votedFor == msg.getcID()))
    {
        votedFor = msg.getcID();
        resetElectionTimeout(); 
        
        // CORREÇÃO: Responder ao CANDIDATO, e não ao "node" (nós mesmos)
        network.sendVoteResponse(sendVoteResponseStruct(candidate, node.getid(), currentTerm, true));
    }
    else
    {
        // CORREÇÃO: Responder a recusa ao CANDIDATO
        network.sendVoteResponse(sendVoteResponseStruct(candidate, node.getid(), currentTerm, false));
    }
}
void raft::collectVotes(int VoterID, int Term, bool granted) 
{
    // Se alguém respondeu com um termo maior que o meu, eu perdi a eleição. Volto a ser seguidor.
    if (Term > currentTerm) {
        currentTerm = Term;
        role = Role::FOLLOWER;
        votedFor = -1;
        
        // 🚨 CORREÇÃO AQUI TAMBÉM: Volto a ser seguidor pacífico e reseto o tempo.
        resetElectionTimeout(); 
        return;
    }

    if (role == Role::CANDIDATE && Term == currentTerm && granted) {
        votesReceived.insert(VoterID);
        
        // Maioria para o total de nós do cluster
        int maioria = (cluster.size() + 1) / 2 + 1;

        if (static_cast<int>(votesReceived.size()) >= maioria) {
            role = Role::LEADER;
            std::cout << "\n [Nó " << node.getid() << "] RECEBEU MAIORIA E SE TORNOU O LÍDER (Termo: " 
                      << currentTerm << ")! \n" << std::endl;
                      
            sendHeartbeats(); 
        }
    }
}

void raft::broadcastClientMessage(ClientCommand msg)
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    if (role == Role::LEADER)
    {
        std::cout << std::put_time(std::localtime(&now_c), "%H:%M:%S") << "\n [Líder " << node.getid() << "] Recebeu comando do cliente: WRITE '" 
                  << msg.getKey() << "' = '" << msg.getValue() << "'" << std::endl;
                  
        log.append(LogEntry(msg, currentTerm));
        ackedLength[node.getid()] = log.getEntries().size();
        
        for (auto follower : cluster)
        {
            replicateLog(follower);
        }
    }
    else
    {
        std::cout << "\n [Nó " << node.getid() << "] Comando recebido, mas não sou o líder. Ignorando/Redirecionando..." << std::endl;
        network.sendClientCommand(sendClientCommandStruct(currentLeader, msg));
    }
}

void raft::replicateLog(NodeInfo follower){
    int followerId = follower.getid();
    
    int prefixLength = sentLength[followerId]; 
    
    auto& originalLog = log.getEntries(); 
    
    std::vector<LogEntry> entries;
    
    if (prefixLength < static_cast<int>(originalLog.size())) {
        entries.assign(originalLog.begin() + prefixLength, originalLog.end());
    }
    
    Log suffix(entries);
    int prefixTerm = 0;
    
    if (prefixLength > 0 && prefixLength <= static_cast<int>(originalLog.size())) {
        prefixTerm = originalLog[prefixLength - 1].getTerm();
    }
    
    network.sendAppendEntries(sendAppendEntriesStruct(
        follower, 
        node.getid(), 
        currentTerm, 
        prefixLength, 
        prefixTerm, 
        commitLength, 
        suffix.getEntries()
    ));
}

void raft::followerReceiveAppendEntries(NodeInfo leader, int term, int prefixLen, int prefixTerm, int leaderCommit, std::vector<LogEntry> suffix){
    // Se o pacote for de um Líder defasado (Termo Menor), rejeitamos IMEDIATAMENTE.
    if (term < currentTerm) {
        network.sendAppendAck(sendAppendAckStruct(leader, node.getid(), currentTerm, 0, false));
        return;
    }

    // Se chegou até aqui, o Líder é legítimo. Reseta o relógio do seguidor.
    lastHeartBeat = std::chrono::steady_clock::now();
    
    // Debug limpo para você auditar a rede
    // std::cout << "[Nó " << node.getid() << "] Recebeu heartbeat do Líder " << leader.getid() << std::endl;

    if (term > currentTerm) {
        currentTerm = term;
        votedFor = -1;
        role = Role::FOLLOWER; // Garante que deixamos de ser Candidatos se necessário
    }

    if (term == currentTerm){
        role = Role::FOLLOWER;
        currentLeader = leader;
    }

    bool logOK = (log.getEntries().size() >= static_cast<size_t>(prefixLen)) && (prefixLen == 0 || log.getEntries()[prefixLen - 1].getTerm() == prefixTerm);    
    if (term == currentTerm && logOK){
        followerAppend(prefixLen, leaderCommit, suffix);
        int ack = prefixLen + suffix.size();
        
        // CRÍTICO: Responder ao líder que o log foi anexado com sucesso
        network.sendAppendAck(sendAppendAckStruct(leader, node.getid(), currentTerm, ack, true));
    }
    else {
        // CRÍTICO: Responder ao líder que o log falhou (Inconsistência de prefixo)
        network.sendAppendAck(sendAppendAckStruct(leader, node.getid(), currentTerm, 0, false));
    }
}

void raft::followerAppend(int prefixLen, int leaderCommit, std::vector<LogEntry> suffix){
    if (!suffix.empty() && log.getEntries().size() > static_cast<size_t>(prefixLen)) {
        int index = log.getEntries().size() > prefixLen + suffix.size() ? prefixLen + suffix.size()-1 : log.getEntries().size()-1;
        if (log.getEntries()[index].getTerm() != suffix[index-prefixLen].getTerm())
            log.truncate(prefixLen);
    }
    
    if(prefixLen + suffix.size() > log.getEntries().size()){
        for (size_t i = log.getEntries().size() - prefixLen; i < suffix.size(); i++) {
            log.append(suffix[i]);
            
            std::cout << " [Nó " << node.getid() << "] Replicou nova entrada no Log: '" 
                      << suffix[i].getOperation().getKey() << "' = '" 
                      << suffix[i].getOperation().getValue() << "'" << std::endl;
        }
    }
    
    if (leaderCommit > commitLength){
        commitLength = leaderCommit; 
    }
}

void raft::logAcknowledgment(int followerID, int term, int ack, bool success){
    if (term == currentTerm && role == Role::LEADER){
        if(success && ack >=ackedLength[followerID]){
            sentLength[followerID] = ack;
            ackedLength[followerID] = ack;
            commitLog();
        }
        else if (sentLength[followerID] > 0){
            NodeInfo followerNode(-1, -1, "");
            for (auto it : cluster)
                if(it.getid() == followerID) followerNode = it;
            sentLength[followerID] -= 1;
            replicateLog(followerNode);
        }
    }
    else if (term > currentTerm){
        currentTerm = term;
        role = Role::FOLLOWER;
        votedFor = -1;
        //cancel election timer
    }
}

void raft::commitLog(){
    while (static_cast<size_t>(commitLength) < log.getEntries().size()) {
    int acks = 0;
    for (auto it : cluster) {
        if (ackedLength[it.getid()] > commitLength)
            acks++;
    }
    
    if (acks >= static_cast<int>((cluster.size() + 1) / 2)) {
        LogEntry committedEntry = log.getEntries()[commitLength];
        applyLogToStateMachine(committedEntry);
        commitLength++;
    } else {
        break;
    }
}
}
void raft::processMessage(std::unique_ptr<messageBase> msg)
{
    if (!msg) return;

    switch (msg->msgtype)
    {
        case messageType::SEND_REQUEST_VOTE:
        {
            auto req = static_cast<sendRequestVoteStruct*>(msg.get());
            // CORREÇÃO: Passar o candidato junto com a mensagem
            receiveElectionMessage(req->msg, req->candidate);
            break;
        }

        case messageType::SEND_VOTE_RESPONSE:
        {
            auto res = static_cast<sendVoteResponseStruct*>(msg.get());
            // Contabiliza o voto recebido (se concedido ou rejeitado)
            collectVotes(res->voterID, res->currentTerm, res->granted);
            break;
        }

        case messageType::SEND_CLIENT_COMMAND:
        {
            auto cmd = static_cast<sendClientCommandStruct*>(msg.get());
            // Repassa o comando do cliente para a máquina de estados / replicação
            broadcastClientMessage(cmd->msg);
            break;
        }
        case messageType::SEND_CLIENT_RESPONSE:
            break;

        case messageType::SEND_APPEND_ENTRIES:
        {
            auto ae = static_cast<sendAppendEntriesStruct*>(msg.get());
            
            // O protocolo precisa do NodeInfo completo do Líder.
            // Vamos procurá-lo no cluster pelo ID enviado na mensagem.
            NodeInfo leaderNode(ae->leaderId, -1, ""); 
            if (node.getid() == ae->leaderId)
            {
                leaderNode = node;
            }
            else
            {
                for (auto& member : cluster)
                {
                    if (member.getid() == ae->leaderId)
                    {
                        leaderNode = member;
                        break;
                    }
                }
            }

            // Invoca a validação e escrita dos logs recebidos do Líder
            followerReceiveAppendEntries(
                leaderNode, 
                ae->currentTerm, 
                ae->prefixLen, 
                ae->prefixTerm, 
                ae->commitLength, 
                ae->suffix
            );
            break;
        }

        case messageType::SEND_APPEND_ACK:
        {
            auto ackMsg = static_cast<sendAppendAckStruct*>(msg.get());
            // Processa a confirmação de escrita de log vinda de um seguidor
            logAcknowledgment(
                ackMsg->followerId, 
                ackMsg->currentTerm, 
                ackMsg->ack, 
                ackMsg->granted
            );
            break;
        }
    }
}
void raft::addClusterMember(const NodeInfo& member) {
    std::lock_guard<std::mutex> lock(stateMutex);
    cluster.push_back(member);
}

// Sorteia um tempo de eleição entre 1500ms e 3000ms (ajustável)
void raft::resetElectionTimeout() {
    std::random_device rd;
    // O operador ^ (XOR) mistura a aleatoriedade do OS com o ID único do seu Nó
    std::mt19937 gen(rd() ^ node.getid()); 
    std::uniform_int_distribution<> distr(1500, 3000); 
    
    timeout = std::chrono::milliseconds(distr(gen));
    lastHeartBeat = std::chrono::steady_clock::now();
    
    // Imprime na tela o tempo exato sorteado para você auditar:
    std::cout << "[Nó " << node.getid() << "] Novo timeout sorteado: " 
              << timeout.count() << "ms" << std::endl;
}

// Inicia o nó de forma assíncrona
void raft::start() {
    if (running) return;
    running = true;

    // 1. Lança a thread de escuta da rede usando o callback unificado
    listenerThread = std::thread([this]() {
        network.startListening(node.getport(), [this](std::unique_ptr<messageBase> msg) {
            // Toda mensagem da rede entra protegida pelo Mutex
            std::lock_guard<std::mutex> lock(stateMutex);
            this->processMessage(std::move(msg));
        });
    });

    // 2. Lança a thread de gerenciamento de tempo (Election Timeout / Heartbeats)
    tickerThread = std::thread(&raft::tickerLoop, this);
    
    std::cout << "[Nó " << node.getid() << "] Inicializado com sucesso." << std::endl;
}

// Desliga o nó limpando as threads paralelas
void raft::stop() {
    if (!running) return;
    running = false;
    
    // Força o encerramento seguro das threads
    if (tickerThread.joinable()) tickerThread.join();
    // Nota: A listenerThread pode ficar travada no accept(). Em produção, fecha-se o socket de fora.
    if (listenerThread.joinable()) listenerThread.detach(); 
}

// O "Coração" do Raft: Roda a cada 100ms checando se o tempo expirou
void raft::tickerLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lock(stateMutex);

        auto now = std::chrono::steady_clock::now();

        if (role == Role::LEADER) {
            // Se eu sou o Líder, envio atualizações/heartbeats periódicos (ex: a cada 500ms)
            static auto lastHeartbeatSent = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeatSent).count() >= 500) {
                sendHeartbeats();
                lastHeartbeatSent = now;
            }
        } 
        else {
            // Se sou Seguidor ou Candidato, verifico se o líder falhou (Timeout)
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartBeat) > timeout) {
                std::cout << "[Nó " << node.getid() << "] Timeout de eleição detectado! Iniciando Eleição..." << std::endl;
                
                // Executa a sua função original de eleição
                // Importante: certifique-se de que dentro de leaderCrashed() você chame resetElectionTimeout()
                leaderCrashed(); 
                resetElectionTimeout(); 
            }
        }
    }
}

// Envia heartbeats vazios para manter a autoridade sobre os seguidores
void raft::sendHeartbeats() {
    for (const auto& follower : cluster) {
        // Envia uma mensagem AppendEntries vazia (sem sufixo) para atuar como Heartbeat
        // Ajuste os parâmetros de acordo com o estado do seu Log
        std::vector<LogEntry> emptySuffix;
        int prefixLen = log.getEntries().size();
        int prefixTerm = (prefixLen > 0) ? log.getEntries()[prefixLen - 1].getTerm() : 0;

        sendAppendEntriesStruct hb(
            follower, 
            node.getid(), 
            currentTerm, 
            prefixLen, 
            prefixTerm, 
            commitLength, 
            emptySuffix
        );
        network.sendAppendEntries(hb);
    }
}
void raft::applyLogToStateMachine(LogEntry entry) {
    ClientCommand cmd = entry.getOperation();
    
    std::vector<NodeInfo> storeCluster = {
        NodeInfo(1, 9001, "10.128.0.4"), 
        NodeInfo(2, 9002, "10.128.0.5"), 
        NodeInfo(3, 9003, "10.128.0.6")  
    };

    QuorumClient storeClient(storeCluster);

    std::cout << "[Nó Raft " << node.getid() << "] Aplicando comando via Quórum no Cluster Store..." << std::endl;

    bool storeSuccess = storeClient.quorumWrite(cmd.getKey(), cmd.getValue());

    if (role == Role::LEADER) {
        ClientInfo client = cmd.getClient();
        std::string statusMsg = storeSuccess ? 
            "OK - Processado pelo Raft e persistido via Quórum (Protocolo 3)" : 
            "ERRO - Falha no quórum do Cluster Store";

        network.sendClientResponse(sendClientResponseStruct(client, statusMsg));
    }
}