#include "Network.h"
#include "NodeInfoSerializer.hpp"
#include "RequestVoteMessageSerializer.hpp"
#include <memory>
#include "LogEntrySerializer.hpp"
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include "ClientCommandSerializer.h"
#include <functional>


bool readAllBytes(int sock, void* buf, size_t size) {
    char* ptr = static_cast<char*>(buf);
    size_t totalRead = 0;
    while (totalRead < size) {
        ssize_t bytesRead = recv(sock, ptr + totalRead, size - totalRead, 0);
        if (bytesRead <= 0) return false; // Conexão fechada ou erro
        totalRead += bytesRead;
    }
    return true;
}

void Network::startListening(int port, std::function<void(std::unique_ptr<messageBase>)> messageHandler)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenfd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind"); close(listenfd); return;
    }

    if (listen(listenfd, 5) < 0) {
        perror("listen"); close(listenfd); return;
    }

    std::cout << "Escutando na porta " << port << std::endl;

    while (true)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        // Este é o socket real da conexão!
        int clientSock = accept(listenfd, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock < 0) { continue; }

        // Passamos o socket correto para a função de leitura
        auto msg = receiveMessage(clientSock);
        
        if (msg && messageHandler) {
            messageHandler(std::move(msg));
        }

        close(clientSock);
    }
}
std::unique_ptr<messageBase> Network::receiveMessage(int clientSock)
{
    messageType type;
    if (!readAllBytes(clientSock, &type, sizeof(type))) return nullptr;

    switch (type)
    {
        case messageType::SEND_CLIENT_RESPONSE:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            
            std::vector<char> statusBuf(size + 1, 0); // +1 para null terminator
            if (!readAllBytes(clientSock, statusBuf.data(), size)) return nullptr;
            
            // Retornamos um ClientInfo vazio pois o cliente que recebe a resposta 
            // só se importa com a string de status.
            return std::make_unique<sendClientResponseStruct>(
                ClientInfo(0, "", 0), 
                std::string(statusBuf.data())
            );
        }
        case messageType::SEND_REQUEST_VOTE:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> nodeBuffer(size);
            if (!readAllBytes(clientSock, nodeBuffer.data(), size)) return nullptr;
            NodeInfo candidate = NodeInfoSerializer::deserialize(nodeBuffer);

            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> msgBuffer(size);
            if (!readAllBytes(clientSock, msgBuffer.data(), size)) return nullptr;
            RequestVoteMessage msg = RequestVoteMessageSerializer::deserialize(msgBuffer);

            return std::make_unique<sendRequestVoteStruct>(NodeInfo(), candidate, msg);
        }

        case messageType::SEND_VOTE_RESPONSE:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> nodeBuffer(size);
            if (!readAllBytes(clientSock, nodeBuffer.data(), size)) return nullptr;
            NodeInfo target = NodeInfoSerializer::deserialize(nodeBuffer);

            int voterID, currentTerm;
            bool granted;
            if (!readAllBytes(clientSock, &voterID, sizeof(voterID))) return nullptr;
            if (!readAllBytes(clientSock, &currentTerm, sizeof(currentTerm))) return nullptr;
            if (!readAllBytes(clientSock, &granted, sizeof(granted))) return nullptr;

            return std::make_unique<sendVoteResponseStruct>(target, voterID, currentTerm, granted);
        }

        case messageType::SEND_APPEND_ENTRIES:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> nodeBuffer(size);
            if (!readAllBytes(clientSock, nodeBuffer.data(), size)) return nullptr;
            NodeInfo target = NodeInfoSerializer::deserialize(nodeBuffer);

            int leaderId, currentTerm, prefixLen, prefixTerm, commitLength, logCount;
            if (!readAllBytes(clientSock, &leaderId, sizeof(leaderId))) return nullptr;
            if (!readAllBytes(clientSock, &currentTerm, sizeof(currentTerm))) return nullptr;
            if (!readAllBytes(clientSock, &prefixLen, sizeof(prefixLen))) return nullptr;
            if (!readAllBytes(clientSock, &prefixTerm, sizeof(prefixTerm))) return nullptr;
            if (!readAllBytes(clientSock, &commitLength, sizeof(commitLength))) return nullptr;
            if (!readAllBytes(clientSock, &logCount, sizeof(logCount))) return nullptr;

            std::vector<LogEntry> suffix;
            for (int i = 0; i < logCount; i++) {
                if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
                std::vector<char> logBuffer(size);
                if (!readAllBytes(clientSock, logBuffer.data(), size)) return nullptr;
                suffix.push_back(LogEntrySerializer::deserialize(logBuffer));
            }

            return std::make_unique<sendAppendEntriesStruct>(target, leaderId, currentTerm, prefixLen, prefixTerm, commitLength, suffix);
        }

        case messageType::SEND_APPEND_ACK:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> nodeBuffer(size);
            if (!readAllBytes(clientSock, nodeBuffer.data(), size)) return nullptr;
            NodeInfo target = NodeInfoSerializer::deserialize(nodeBuffer);

            int followerId, currentTerm, ack;
            bool granted;
            if (!readAllBytes(clientSock, &followerId, sizeof(followerId))) return nullptr;
            if (!readAllBytes(clientSock, &currentTerm, sizeof(currentTerm))) return nullptr;
            if (!readAllBytes(clientSock, &ack, sizeof(ack))) return nullptr;
            if (!readAllBytes(clientSock, &granted, sizeof(granted))) return nullptr;

            return std::make_unique<sendAppendAckStruct>(target, followerId, currentTerm, ack, granted);
        }

        case messageType::SEND_CLIENT_COMMAND:
        {
            int size;
            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> nodeBuffer(size);
            if (!readAllBytes(clientSock, nodeBuffer.data(), size)) return nullptr;
            NodeInfo target = NodeInfoSerializer::deserialize(nodeBuffer);

            if (!readAllBytes(clientSock, &size, sizeof(size))) return nullptr;
            std::vector<char> commandBuffer(size);
            if (!readAllBytes(clientSock, commandBuffer.data(), size)) return nullptr;
            ClientCommand command = ClientCommandSerializer::deserialize(commandBuffer);

            return std::make_unique<sendClientCommandStruct>(target, command);
        }
    }
    return nullptr;
}

int Network::createConnection(const std::string& ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0)
    {
        perror("socket");
        return -1;
    }


    sockaddr_in server{};

    server.sin_family = AF_INET;
    server.sin_port = htons(port);


    inet_pton(
        AF_INET,
        ip.c_str(),
        &server.sin_addr
    );


    if(connect(
        sock,
        (sockaddr*)&server,
        sizeof(server)
    ) < 0)
    {
        perror("connect");
        close(sock);
        return -1;
    }


    return sock;
}

void Network::sendRequestVote(sendRequestVoteStruct msg)
{
    int sock = createConnection(msg.target.getaddress(), msg.target.getport());
    if(sock < 0) return;

    messageType type = messageType::SEND_REQUEST_VOTE;
    send(sock, &type, sizeof(type), 0);

    // CORREÇÃO: Enviar o NodeInfo do candidato primeiro (O recv() do outro lado exige isso!)
    auto nodeBuffer = NodeInfoSerializer::serialize(msg.candidate);
    int nodeSize = nodeBuffer.size();
    send(sock, &nodeSize, sizeof(nodeSize), 0);
    send(sock, nodeBuffer.data(), nodeSize, 0);

    // Depois envia a mensagem real de RequestVote
    auto data = RequestVoteMessageSerializer::serialize(msg.msg);
    int size = data.size();
    send(sock, &size, sizeof(size), 0);
    send(sock, data.data(), size, 0);

    close(sock);
}

void Network::sendClientResponse(sendClientResponseStruct msg)
{
    int sock = createConnection(msg.targetClient.ip, msg.targetClient.port);
    if (sock < 0) return;

    messageType type = messageType::SEND_CLIENT_RESPONSE;
    send(sock, &type, sizeof(type), 0);

    // Envia o status (ex: "OK")
    int statusSize = msg.status.size();
    send(sock, &statusSize, sizeof(statusSize), 0);
    send(sock, msg.status.c_str(), statusSize, 0);

    close(sock);
}

void Network::sendClientCommand(sendClientCommandStruct msg)
{
    int sock = createConnection(
        msg.target.getaddress(),
        msg.target.getport()
    );

    if (sock < 0)
        return;

    messageType type = messageType::SEND_CLIENT_COMMAND;
    send(sock, &type, sizeof(type), 0);

    auto nodeBuffer = NodeInfoSerializer::serialize(msg.target);

    int nodeSize = nodeBuffer.size();
    send(sock, &nodeSize, sizeof(nodeSize), 0);
    send(sock, nodeBuffer.data(), nodeSize, 0);

    auto commandBuffer = ClientCommandSerializer::serialize(msg.msg);

    int commandSize = commandBuffer.size();
    send(sock, &commandSize, sizeof(commandSize), 0);
    send(sock, commandBuffer.data(), commandSize, 0);

    close(sock);
}

void Network::sendVoteResponse(sendVoteResponseStruct msg)
{
    int sock = createConnection(
        msg.target.getaddress(),
        msg.target.getport()
    );

    if (sock < 0)
        return;

    messageType type = messageType::SEND_VOTE_RESPONSE;
    send(sock, &type, sizeof(type), 0);

    std::vector<char> nodeBuffer =
        NodeInfoSerializer::serialize(msg.target);

    int size = nodeBuffer.size();

    send(sock, &size, sizeof(size), 0);
    send(sock, nodeBuffer.data(), size, 0);

    send(sock, &msg.voterID, sizeof(msg.voterID), 0);
    send(sock, &msg.currentTerm, sizeof(msg.currentTerm), 0);
    send(sock, &msg.granted, sizeof(msg.granted), 0);

    close(sock);
}

void Network::sendAppendAck(sendAppendAckStruct msg)
{
    int sock = createConnection(
        msg.target.getaddress(),
        msg.target.getport()
    );

    if (sock < 0)
        return;

    messageType type = messageType::SEND_APPEND_ACK;
    send(sock, &type, sizeof(type), 0);

    std::vector<char> nodeBuffer =
        NodeInfoSerializer::serialize(msg.target);

    int size = nodeBuffer.size();

    send(sock, &size, sizeof(size), 0);
    send(sock, nodeBuffer.data(), size, 0);

    send(sock, &msg.followerId, sizeof(msg.followerId), 0);
    send(sock, &msg.currentTerm, sizeof(msg.currentTerm), 0);
    send(sock, &msg.ack, sizeof(msg.ack), 0);
    send(sock, &msg.granted, sizeof(msg.granted), 0);

    close(sock);
}

void Network::sendAppendEntries(sendAppendEntriesStruct msg)
{
    int sock = createConnection(
        msg.target.getaddress(),
        msg.target.getport()
    );

    if (sock < 0)
        return;

    messageType type = messageType::SEND_APPEND_ENTRIES;
    send(sock, &type, sizeof(type), 0);

    // Envia o NodeInfo
    std::vector<char> nodeBuffer =
        NodeInfoSerializer::serialize(msg.target);

    int size = nodeBuffer.size();

    send(sock, &size, sizeof(size), 0);
    send(sock, nodeBuffer.data(), size, 0);

    // Envia os campos da mensagem
    send(sock, &msg.leaderId, sizeof(msg.leaderId), 0);
    send(sock, &msg.currentTerm, sizeof(msg.currentTerm), 0);
    send(sock, &msg.prefixLen, sizeof(msg.prefixLen), 0);
    send(sock, &msg.prefixTerm, sizeof(msg.prefixTerm), 0);
    send(sock, &msg.commitLength, sizeof(msg.commitLength), 0);

    // Envia a quantidade de entradas do log
    int logCount = msg.suffix.size();
    send(sock, &logCount, sizeof(logCount), 0);

    // Envia cada LogEntry
    for (auto& entry : msg.suffix)
    {
        std::vector<char> logBuffer =
            LogEntrySerializer::serialize(entry);

        size = logBuffer.size();

        send(sock, &size, sizeof(size), 0);
        send(sock, logBuffer.data(), size, 0);
    }

    close(sock);
}