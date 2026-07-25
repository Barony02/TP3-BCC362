#include <string>
class ClientInfo {
public:
    int id;
    int port;
    std::string ip;
    
    ClientInfo(int port, std::string ip, int id): id(id), port(port), ip(ip){}
};