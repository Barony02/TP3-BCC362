#ifndef REQUESTVOTEMESSAGE_HPP
#define REQUESTVOTEMESSAGE_HPP

class RequestVoteMessage {
private:
    int cID;
    int cTerm;
    int cLogLength;
    int cLogTerm;

public:
    // Construtor vazio necessário para serialização
    RequestVoteMessage() : cID(-1), cTerm(-1), cLogLength(-1), cLogTerm(-1) {}

    // Construtor com a ordem EXATA que estamos usando no raft.cpp
    RequestVoteMessage(int id, int term, int logLength, int logTerm) {
        cID = id;
        cTerm = term;
        cLogLength = logLength;
        cLogTerm = logTerm;
    }

    // Getters blindados
    int getcID() const { return cID; }
    int getcTerm() const { return cTerm; }
    int getcLogLength() const { return cLogLength; }
    int getcLogTerm() const { return cLogTerm; }
};

#endif