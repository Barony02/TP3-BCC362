#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <string>
#include "ClientInfo.h"

enum class Operation
{
    WRITE
};

class ClientCommand
{
private:
    ClientInfo client; 
    Operation op;
    std::string key;
    std::string value;

public:
    ClientCommand(ClientInfo client, Operation op, const std::string &key, const std::string &value) 
        : client(client), op(op), key(key), value(value) {}

    // A palavra-chave 'const' garante que o método não altera o estado da classe,
    // permitindo que o ClientCommandSerializer utilize a classe em modo "somente leitura".
    ClientInfo getClient() const { return client; } 
    Operation getOperation() const { return op; }
    std::string getKey() const { return key; }
    std::string getValue() const { return value; }
};

class LogEntry
{
    int term;
    ClientCommand operation;

public:
    // A ordem de inicialização foi corrigida para respeitar a declaração (term antes de operation)
    LogEntry(ClientCommand Operation, int Term) : term(Term), operation(Operation) {}
    
    int getTerm() const { return term; }
    ClientCommand getOperation() const { return operation; }
};

#endif