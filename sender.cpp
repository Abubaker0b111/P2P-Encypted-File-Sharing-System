#include<iostream>
#include<cstring>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<cstdio>
#include"packet.h"
#include<thread>
#include<chrono>
#include<sys/time.h>

#define PORT 8080 // setting the default port to 8080
#define SERVER_IP "127.0.0.1" //Setting the server ip address. It as the same as this device for now
#define BUFFER_SIZE 1024// Setting the default buffer size to 1024 characters

int main(){
    int sockfd;
    struct sockaddr_in server_addr, client_addr;

    struct timeval tv;
    //Setting timeout values
    tv.tv_sec = 1;  // 1 second
    tv.tv_usec = 0; //0 milisecond

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){//Creating the socket
        perror("Socket Creation Error\n");
        return -1;
    }

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){//Setting up the socket to timeout after the given time value
        perror("Error setting timeout\n");
        return -1;
    }

    //setting the ip address type and port for the server address
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

    bool established = false;

    int req_limit = 10;

    while(!established && req_limit > 0){// Trying to establish a three way handshake before communicating

        std::cout<<"Sending SYN...\n";

        sendto(sockfd, &syn_pkt, sizeof(Header), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));//Sending the initial SYN packet

        Packet reply;

        memset(&reply, 0, sizeof(reply));
        socklen_t len = sizeof(server_addr);

        int n = recvfrom(sockfd, &reply, sizeof(reply), 0, (struct sockaddr *)&server_addr, &len);//Waiting for the subsequent SYN-ACK reply

        if(n > 0 && reply.header.flags == (SYN | ACK)){
            std::cout << "Received SYN-ACK. Sending final ACK...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));//Adding artificial delay for observing the handshake process in real time
            Packet ack_pkt;
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.header.flags = ACK;
            
            sendto(sockfd, &ack_pkt, sizeof(Header), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));//Sending the final ACK response
            
            std::cout << "Connection ESTABLISHED!" << std::endl;
            established = true;
        }
        else{//Invoking Timeout if no Reply is received within the time limit
            std::cout<<"[Sender] ACK Timeout! Retransmitting SYN request"<<std::endl;
            req_limit--;
        }
    }

    if(req_limit <= 0){//Ends the program if too many Timeouts occur
        std::cout<<"[Sender] Request Limit Exceeded! Could not connect to server\n";
        return -1;
    }
    

    int total_packets = 5;

    /*Test the sequence logic by sending 5 packets in sequence and waitng for the response*/
    for(int current_seq = 1 ; current_seq <= total_packets ; current_seq++){
        Packet data_pkt;

        memset(&data_pkt, 0, sizeof(data_pkt));

        data_pkt.header.flags = DATA;
        data_pkt.header.seq_num = htonl(current_seq);

        std::string msg = "This is a message for sequence number: " + std::to_string(current_seq);
        
        strncpy(data_pkt.payload, msg.c_str(), MAX_PAYLOAD);

        size_t packet_size = sizeof(Header) + msg.length();

        uint checksum = calculate_checksum(&data_pkt, packet_size);

        std::cout<<"[Sender] Sending packet with CheckSum: "<<checksum<<std::endl;
        data_pkt.header.checksum = checksum;

        bool ack_received = false;
        bool need_transmission = true;
        while(!ack_received){// Keeps retransmitting the packet untill the approprite ACK is received
            int n = 0;

            if(need_transmission){
                std::cout<<"[Sender] Sending packet seq: "<<current_seq<<std::endl;
                sendto(sockfd, &data_pkt, packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));//Sending the initial DATA packet
                need_transmission = false;
            }

            Packet ack_reply;
            socklen_t len = sizeof(server_addr);
            n = recvfrom(sockfd, &ack_reply, sizeof(ack_reply), 0, (struct sockaddr *)&server_addr, &len);//Waiting for the subsequene response

            if(n > 0){
                uint8_t flags = ack_reply.header.flags;
                uint32_t ack_num = ntohl(ack_reply.header.ack_num);

                if((flags & ACK) && ack_num == current_seq){
                    std::cout<<"[Sender] Received ACK for SEQ: "<<current_seq<<std::endl;
                    ack_received = true;
                }
                else{
                    std::cout<<"[Sender] Received Unexpected Packet\n";
                }
            }
            else{
                std::cout<<"[Sender] Timeout! Retransmitting Packet Sequence: "<<current_seq<<std::endl;
                need_transmission = true;
            }
        }
    }
    close(sockfd);

    return 0;
}