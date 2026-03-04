#include<iostream>
#include<cstring>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<cstdio>
#include"packet.h"
#include<thread>
#include<chrono>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main(){
    int sockfd;
    struct sockaddr_in server_addr, client_addr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket Creation Error\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if((inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr)) <= 0){
        perror("Invalid Address / Adress not supported\n");
        return -1;
    }

    Packet syn_pkt;

    memset(&syn_pkt, 0, sizeof(syn_pkt));
    syn_pkt.header.flags = SYN;

    std::cout<<"Sending SYN...\n";

    sendto(sockfd, &syn_pkt, sizeof(Header), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

    Packet reply;

    memset(&reply, 0, sizeof(reply));
    socklen_t len = sizeof(server_addr);

    int n = recvfrom(sockfd, &reply, sizeof(reply), 0, (struct sockaddr *)&server_addr, &len);

    if(n > 0 && reply.header.flags == (SYN | ACK)){
        std::cout << "Received SYN-ACK. Sending final ACK...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        Packet ack_pkt;
        memset(&ack_pkt, 0, sizeof(ack_pkt));
        ack_pkt.header.flags = ACK;
        
        sendto(sockfd, &ack_pkt, sizeof(Header), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        
        std::cout << "Connection ESTABLISHED!" << std::endl;
        }

    close(sockfd);

    return 0;
}