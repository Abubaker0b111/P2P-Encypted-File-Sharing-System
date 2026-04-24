#ifndef NAT_TRAVERSAL_H
#define NAT_TRAVERSAL_H

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>

class NatTraversal {
private:
    std::string server_ip;
    int server_port;
    std::string my_name;
    
    int sockfd;
    struct sockaddr_in server_addr;

    bool PunchHole(struct sockaddr_in& peer_addr, const std::string& peer_ip, int peer_port);

public:
    NatTraversal(const std::string& stun_ip, int stun_port, const std::string& username);
    ~NatTraversal();

    bool InitializeAndRegister();
    int GetSocket() const;
    void SendPing();
    void RequestActiveList();
    void RequestConnection(const std::string& target_name);
    bool ProcessServerMessage(const std::string& msg, struct sockaddr_in& out_peer_addr, bool& out_is_caller);
};

#endif