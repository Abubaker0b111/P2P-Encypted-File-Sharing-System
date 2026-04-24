#include <iostream>
#include <sys/select.h>
#include <unistd.h>
#include <string>
#include "crypto_manager.h"
#include "nat_traversal.h"
#include "rudp_socket.h"
#include "file_agent.h"

// Replace this with the public IP of the bootstrap server
#define SERVER_IP "127.0.0.1" 
#define SERVER_PORT 9000


//Simple function to print the menu
void PrintMenu(const std::string& my_name) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "   SECURE P2P NODE : " << my_name << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " [1] View Online Peers" << std::endl;
    std::cout << " [2] Connect & Send File" << std::endl;
    std::cout << " [3] Quit" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Select option: " << std::flush;
}

void ExecuteP2PTransfer(int raw_socket, struct sockaddr_in peer_addr, bool is_caller) {
    CryptoManager crypto;
    if (!crypto.Initialize()) return;

    RUDPSocket reliable_channel;
    reliable_channel.Attach(raw_socket, peer_addr);//Handle the raw udp socket to the reliable channel

    // Perform Handshake
    if (!crypto.PerformKeyExchange(reliable_channel, is_caller)) return;
    reliable_channel.EnableEncryption(crypto.GetTxKey(), crypto.GetRxKey());

    FileAgent file_app;
    
    if (is_caller) {
        //Sender Process
        std::string filepath;
        std::cout << "\n==========================================" << std::endl;
        std::cout << " [SYSTEM] SECURE CONNECTION ESTABLISHED" << std::endl;
        std::cout << "==========================================" << std::endl;
        std::cout << "Enter the path/name of the file to send: ";
        
        //Get the path of the file to transfer from the user
        std::getline(std::cin, filepath);

        if (!filepath.empty()) {
            file_app.SendFile(reliable_channel, filepath); 
        } 
        else {
            std::cout << "[System] Transfer cancelled." << std::endl;
        }
    } 
    else {
        // Receiver Process
        std::cout << "\n[System] Secure connection established. Waiting for peer to send file..." << std::endl;
        file_app.ReceiveFile(reliable_channel);
    }
    
    std::cout << "\n[System] Transfer sequence complete. Press ENTER to return to menu." << std::endl;
}

int main(int argc, char *argv[]) {

    //Check if the correct number of arguments were passed
    if (argc < 2) {
        std::cout << "Usage: ./client <my_name>" << std::endl;
        return -1;
    }

    std::string my_name = argv[1];

    NatTraversal nat(SERVER_IP, SERVER_PORT, my_name);

    //connect and register with the bootstrap server
    if (!nat.InitializeAndRegister()) {
        std::cerr << "Failed to connect to Bootstrap Server." << std::endl;
        return -1;
    }

    int sockfd = nat.GetSocket();
    fd_set readfds;
    char net_buffer[1024];

    PrintMenu(my_name);

    // The Background Event Loop
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // Listen to Keyboard
        FD_SET(sockfd, &readfds);       // Listen to Network

        struct timeval tv;
        tv.tv_sec = 30; // 30-second heartbeat
        tv.tv_usec = 0;

        int activity = select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        // Send Keep Ailve Ping to Server if idle
        if (activity == 0) {
            nat.SendPing();
            continue;
        }

        //Handle keyboard Input 
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::string input;
            std::getline(std::cin, input);

            if (input == "1") {
                nat.RequestActiveList();
            } 
            else if (input == "2") {// Connecting with the peer
                std::cout << "\nEnter the name of the peer to connect to: ";
                std::string target;
                std::getline(std::cin, target);
                
                if (!target.empty()) {
                    std::cout << "[System] Initiating connection to " << target << "..." << std::endl;
                    nat.RequestConnection(target);
                }
                else {
                    PrintMenu(my_name);
                }
            } 
            else if (input == "3" || input == "quit") {
                std::cout << "[System] Shutting down node." << std::endl;
                break;
            } 
            else {
                PrintMenu(my_name);
            }
        }

        //Handle incoming traffic
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            int n = recvfrom(sockfd, net_buffer, sizeof(net_buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);

            if (n > 0) {
                net_buffer[n] = '\0';
                struct sockaddr_in peer_addr;
                bool is_caller = false;

                // Process the message and take appropriate action
                if (nat.ProcessServerMessage(net_buffer, peer_addr, is_caller)) {
                    
                    nat.SendPing();
                    // Initiate the File transfer after a direct p2p connection established
                    ExecuteP2PTransfer(sockfd, peer_addr, is_caller);
                    nat.SendPing();

                    PrintMenu(my_name); 
                }
            }
        }
    }

    return 0;
}