#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <string>
#include <sstream>
#include <chrono>

#define SERVER_PORT 9000
//Default TTL of peers set to 60 seconds
#define TIMEOUT_SECONDS 60


struct PeerInfo {
    struct sockaddr_in addr;// Address of peer
    std::chrono::steady_clock::time_point last_seen; // Time of last received ping
};

std::map<std::string, PeerInfo> active_peers;// Hashmap to hold all the active peers

std::string AddrToString(struct sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

//Function to clear the active_peer list of inactive peers
void CleanStalePeers() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = active_peers.begin(); it != active_peers.end(); ) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_seen).count();
        if (duration > TIMEOUT_SECONDS) {// Check if the peer is expired or not
            //std::cout << "[Server] Peer '" << it->first << "' timed out and was removed." << std::endl;
            it = active_peers.erase(it);// remove inactive peer
        } 
        else {
            ++it;
        }
    }
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[1024];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return -1;

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) return -1;

    std::cout << "[Rendezvous] Stateful Matchmaker active on port " << SERVER_PORT << std::endl;

    while (true) {
        socklen_t len = sizeof(clientAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&clientAddr, &len);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            std::istringstream iss(message);
            std::string command, arg;
            
            iss >> command >> arg;

            // Before Processing any command clear the list of dead peers
            CleanStalePeers();

            auto now = std::chrono::steady_clock::now();// Save the current time

            if (command == "REGISTER" && !arg.empty()) {
                active_peers[arg] = {clientAddr, now};// Register the new peer and save its address along with current time
                std::cout << "[Server] Registered: " << arg << " at " << AddrToString(clientAddr) << std::endl;
                std::string reply = "SUCCESS: Registered as " + arg;
                sendto(sockfd, reply.c_str(), reply.length(), 0, (struct sockaddr *)&clientAddr, len);
            }
            
            else if (command == "PING" && !arg.empty()) {
                if (active_peers.find(arg) != active_peers.end()) {// Check if the peer is registered
                    active_peers[arg].last_seen = now; // Refresh their timer
                    active_peers[arg].addr = clientAddr; // Update IP in case they roamed networks
                }
            }

            else if (command == "LIST") {// Sends the list of active peers to the sender
                std::string active_list = "ACTIVE_PEERS ";
                for (const auto& peer : active_peers) {
                    active_list += peer.first + ",";
                }
                sendto(sockfd, active_list.c_str(), active_list.length(), 0, (struct sockaddr *)&clientAddr, len);
            }
            
            else if (command == "CONNECT" && !arg.empty()) {// Sends the IP:port of the requested peer
                if (active_peers.find(arg) != active_peers.end()) {
                    struct sockaddr_in targetAddr = active_peers[arg].addr;
                    
                    std::string replyToRequester = "PEER_INFO " + AddrToString(targetAddr);
                    sendto(sockfd, replyToRequester.c_str(), replyToRequester.length(), 0, (struct sockaddr *)&clientAddr, len);
                    
                    std::string alertToTarget = "INCOMING " + AddrToString(clientAddr);
                    sendto(sockfd, alertToTarget.c_str(), alertToTarget.length(), 0, (struct sockaddr *)&targetAddr, sizeof(targetAddr));
                    
                    std::cout << "[Server] Match made between requestor and " << arg << std::endl;
                } 
                else {// Send Error message if peer is offline
                    std::string error = "ERROR: Peer offline";
                    sendto(sockfd, error.c_str(), error.length(), 0, (struct sockaddr *)&clientAddr, len);
                }
            }
        }
    }
    return 0;
}