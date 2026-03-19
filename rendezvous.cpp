#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <string>
#include <sstream>

#define SERVER_PORT 9000

// A map to store Username -> Client Address
std::map<std::string, struct sockaddr_in> active_peers;

// Helper to convert sockaddr_in to a readable string (IP:Port)
std::string AddrToString(struct sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[1024];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    std::cout << "[Rendezvous] Matchmaker active on port " << SERVER_PORT << std::endl;

    while (true) {
        socklen_t len = sizeof(clientAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&clientAddr, &len);
        
        if (n > 0) {
            buffer[n] = '\0'; // Null-terminate the string
            std::string message(buffer);
            std::istringstream iss(message);
            std::string command, arg;
            
            iss >> command >> arg; // Split into "COMMAND" and "Argument"

            // 1. Handle "REGISTER <username>"
            if (command == "REGISTER" && !arg.empty()) {
                active_peers[arg] = clientAddr;
                std::cout << "[Server] Registered Peer: " << arg << " at " << AddrToString(clientAddr) << std::endl;
                
                std::string reply = "SUCCESS: Registered as " + arg;
                sendto(sockfd, reply.c_str(), reply.length(), 0, (struct sockaddr *)&clientAddr, len);
            }
            
            // 2. Handle "CONNECT <target_username>"
            else if (command == "CONNECT" && !arg.empty()) {
                std::cout << "[Server] Connection request for: " << arg << std::endl;
                
                if (active_peers.find(arg) != active_peers.end()) {
                    struct sockaddr_in targetAddr = active_peers[arg];
                    
                    // A. Send Target's info to the Requester
                    std::string replyToRequester = "PEER_INFO " + AddrToString(targetAddr);
                    sendto(sockfd, replyToRequester.c_str(), replyToRequester.length(), 0, (struct sockaddr *)&clientAddr, len);
                    
                    // B. Send Requester's info to the Target (Triggering the simultaneous punch)
                    std::string alertToTarget = "INCOMING " + AddrToString(clientAddr);
                    sendto(sockfd, alertToTarget.c_str(), alertToTarget.length(), 0, (struct sockaddr *)&targetAddr, sizeof(targetAddr));
                    
                    std::cout << "[Server] Match made! Instructions dispatched." << std::endl;
                } else {
                    std::string error = "ERROR: Peer not found";
                    sendto(sockfd, error.c_str(), error.length(), 0, (struct sockaddr *)&clientAddr, len);
                }
            }
        }
    }

    close(sockfd);
    return 0;
}