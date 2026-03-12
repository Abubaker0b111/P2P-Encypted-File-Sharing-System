#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include"packet.h"
#include<thread>
#include<chrono>
#include<ctime>
#include<cstdlib>

#define PORT 8080 //Setting the default port to 8080
#define BUFFER_SIZE 1024 //Setting the default buffer size to 1024

int main(){
    srand(time(0)); //seeding the random number generator

    int sockfd; //socket file descriptor
    struct sockaddr_in server_addr, client_addr; // Structure for server and client addresses
    Packet buffer;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){ // Creating the socket
        perror("Socket Creation Failed!\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; //Setting the ip address type as ipv4
    server_addr.sin_addr.s_addr = INADDR_ANY; //Setting the input so that the server listens for input from all available channels
    server_addr.sin_port = htons(PORT); //Setting the port after convert it from host to network format

    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){// Binding the socket to the specific address and port
        perror("Bind Failed!\n");
        return -1;
    }

    std::cout<<"Receiver is listening on Port "<<PORT<<"..."<<std::endl;
    int n = 0;
    socklen_t len = sizeof(client_addr);
    while(1){// Establishing a connection with the connection using a three way handshake
        n = recvfrom(sockfd, &buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &len); // Waiting for request from client

        uint8_t flags = buffer.header.flags; //Reading the flags from the received packet

        if(flags == SYN){ //If the packet is a SYN packet then we send an ACK back
            std::cout<<"Received SYN. Sending SYN-ACK...\n";
            //std::this_thread::sleep_for(std::chrono::seconds(2));//Adding an artificial delay so the handshake process can be seen in real time
            Packet reply;
            memset(&reply, 0, sizeof(reply));

            reply.header.flags = SYN | ACK;

            sendto(sockfd, &reply, sizeof(Header), 0, (struct sockaddr *)&client_addr, len);
        }
        else if(flags == ACK){ // IF the packet is and ACK packet then we establish the connection
            std::cout<<"Received ACK. Connection Established\n";
            break;
        }
    }
    
    while(true){
        Packet incoming;

        n = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr *)&client_addr, &len);// Waintng for incoming packets

        if(n > 0){

            int corruption_chance = rand()%10 + 1;

            if(corruption_chance <= 2){
                incoming.payload[0] ^= 0xff;
            }

            uint16_t validation = calculate_checksum(&incoming, n);
            std::cout<<"Received checksum: "<<validation<<std::endl;
            if(validation != 0){
                std::cout<<"Checksum Failed! Packet is corrupted\nDropping Packet\n";
                continue;
            }

            uint8_t flags = incoming.header.flags;// Reading the flags for the packet
            uint32_t seq = ntohl(incoming.header.seq_num);// Reading the sequence number of the packet

            if(flags & DATA){

                int payload_len = n - sizeof(Header);
                char msg[MAX_PAYLOAD + 1];
                memcpy(msg, incoming.payload, payload_len);

                msg[payload_len] = '\0';

                std::cout<<"[Receiver] Received Packet Seq: "<<seq<<" -> "<<msg<<std::endl;
                Packet ack_pkt;

                memset(&ack_pkt, 0, sizeof(ack_pkt));
                ack_pkt.header.flags = ACK;
                ack_pkt.header.ack_num = htonl(seq);

                std::cout<<"[Receiver] Sending the ACK...\n";

                sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr, len);// Sending the appropriate ack sequence back
            }
        }
    }
    close(sockfd);
    return 0;
}
