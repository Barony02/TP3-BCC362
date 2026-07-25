#ifndef NODEINFO_H
#define NODEINFO_H

#include <string>

class NodeInfo{
    std::string address;
    int id;
    int port;
public:
    NodeInfo(int id, int port, std::string address){
        this->address = address;
        this->id = id;
        this-> port = port;
    }
    NodeInfo(): address(""), id(-1), port(-1){}
    void setid(int ID){id = ID;}
    void setport(int Port) {port = Port;}
    void setaddress(std::string addr){address = addr;}
    int getid() const {return id;}
    int getport() const {return port;}
    std::string getaddress() const {return address;}

};

#endif