#include "nat_traversal.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

//initializing the basic fields
NatTraversal::NatTraversal(const std::string& stun_ip, int stun_port, const std::string& username) {
    this->server_ip = stun_ip;
    this->server_port = stun_port;
    this->my_name = username;
    this->sockfd = -1;
}

NatTraversal::~NatTraversal() {}

//Initialze the socket and register the peer with the bootstrap server
bool NatTraversal::InitializeAndRegister() {
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return false;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    std::string reg_msg = "REGISTER " + my_name;
    sendto(sockfd, reg_msg.c_str(), reg_msg.length(), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    return true;
}

int NatTraversal::GetSocket() const { 
    return sockfd; 
}

//Pings the server
void NatTraversal::SendPing() {
    std::string ping = "PING " + my_name;
    sendto(sockfd, ping.c_str(), ping.length(), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

//Requests a list of all the active peers from the bootstrap server
void NatTraversal::RequestActiveList() {
    std::string list_req = "LIST " + my_name;
    sendto(sockfd, list_req.c_str(), list_req.length(), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

//Requests the server for the IP and Port of a target peer
void NatTraversal::RequestConnection(const std::string& target_name) {
    std::string conn_msg = "CONNECT " + target_name;
    sendto(sockfd, conn_msg.c_str(), conn_msg.length(), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

//Establishes direct connection to the peer
bool NatTraversal::PunchHole(struct sockaddr_in& peer_addr, const std::string& peer_ip, int peer_port) {
    //std::cout << "\n[NAT] INITIATING UDP HOLE PUNCH" << std::endl;
    
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip.c_str(), &peer_addr.sin_addr);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));//Setting a timeout val of 1 second

    std::string punch_msg = "PUNCH_FROM_" + my_name;
    char buffer[1024];
    int attempts = 10;
    
    while (attempts > 0) {
        sendto(sockfd, punch_msg.c_str(), punch_msg.length(), 0, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
        socklen_t peer_len = sizeof(peer_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&peer_addr, &peer_len);
        
        if (n > 0) {
            std::cout << "\n===================================" << std::endl;
            std::cout << " SUCCESS! DIRECT P2P ESTABLISHED!" << std::endl;
            std::cout << "===================================\n" << std::endl;
            sendto(sockfd, punch_msg.c_str(), punch_msg.length(), 0, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
            
            tv.tv_sec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));//Disable the timeout
            return true;
        }
        attempts--;
    }
    
    std::cerr << "[NAT] Hole punch failed." << std::endl;
    return false;
}

bool NatTraversal::ProcessServerMessage(const std::string& msg, struct sockaddr_in& out_peer_addr, bool& out_is_caller) {
    if (msg.find("ACTIVE_PEERS") != std::string::npos) {//Print the list of active peers
        std::cout << "\n--- Online Peers ---\n" << msg.substr(13) << "\n--------------------\n> " << std::flush;
        return false;
    }
    else if (msg.find("SUCCESS:") != std::string::npos || msg.find("ERROR:") != std::string::npos) {
        std::cout << "\n[Server] " << msg << "\n> " << std::flush;
        return false;
    }
    
    bool trigger_punch = false;
    std::string peer_ip = "";
    int peer_port = 0;
    
    //The server returned the peers info
    //Extract the ip and port and initiate the hole punch
    if (msg.find("PEER_INFO") != std::string::npos) {
        out_is_caller = true;
        trigger_punch = true;
        size_t space_idx = msg.find(' ');
        size_t colon_idx = msg.find(':');
        peer_ip = msg.substr(space_idx + 1, colon_idx - space_idx - 1);
        peer_port = std::stoi(msg.substr(colon_idx + 1));
        //std::cout << "\n[NAT] Target located at " << peer_ip << ":" << peer_port << std::endl;
    }
    //The server informed us the we have a connection request incoming. 
    //Extract the ip and port and initiate the hole punch
    else if (msg.find("INCOMING") != std::string::npos) {
        out_is_caller = false;
        trigger_punch = true;
        size_t space_idx = msg.find(' ');
        size_t colon_idx = msg.find(':');
        peer_ip = msg.substr(space_idx + 1, colon_idx - space_idx - 1);
        peer_port = std::stoi(msg.substr(colon_idx + 1));
        //std::cout << "\n[NAT] Incoming connection from " << peer_ip << ":" << peer_port << std::endl;
    }

    //Initiate the hole punch
    if (trigger_punch) {
        return PunchHole(out_peer_addr, peer_ip, peer_port);
    }
    
    return false;
}