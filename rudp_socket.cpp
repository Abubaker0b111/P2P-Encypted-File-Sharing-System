#include"rudp_socket.h"

RUDPSocket::RUDPSocket(){
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket Creation Error!\n");
        return;
    }
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET; //Setting the ip address type as ipv4
    targetAddr.sin_addr.s_addr = INADDR_ANY; //Setting the input so that the server listens for input from all available channels

    target_len = sizeof(targetAddr);
    current_seq = 1;
    expected_seq = 1;
    setTimeout(1);
}

RUDPSocket::~RUDPSocket(){
    close(sockfd);
}

uint16_t RUDPSocket::calculate_checksum(void* vdata, size_t length){
    char *data = (char*)vdata;
    uint32_t acc = 0xffff;

    for(size_t i = 0 ; i<length ; i += 2){// Adding one word at a time
        uint16_t word = 0;
        memcpy(&word, data+i, 2);
        acc += ntohs(word);

        if(acc > 0xffff){
            acc -= 0xffff;
        }
    }

    if(length & 1){//Accounting for the last byte if the length is odd
        uint16_t word = 0;
        memcpy(&word, data + length -1 , 1);

        acc += word;

        if(acc > 0xffff){
            acc -= 0xffff;
        }
    }

    return (~acc);
}

void RUDPSocket::setTimeout(int seconds){
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){//Setting up the socket to timeout after the given time value
        perror("Error setting timeout\n");
        return;
    }
}

bool RUDPSocket::Connect(const std::string& ip, int port){
    targetAddr.sin_port = htons(port);

    if((inet_pton(AF_INET, ip.c_str(), &targetAddr.sin_addr)) <= 0){
        perror("Invalid Address / Adress not supported\n");
        return false;
    }

    Packet syn_pkt;

    memset(&syn_pkt, 0, sizeof(syn_pkt));
    syn_pkt.header.flags = SYN;

    bool established = false;

    int req_limit = 10;

    while(!established && req_limit > 0){// Trying to establish a three way handshake before communicating

        std::cout<<"Sending SYN...\n";

        sendto(sockfd, &syn_pkt, sizeof(Header), 0, (const struct sockaddr *)&targetAddr, sizeof(targetAddr));//Sending the initial SYN packet

        Packet reply;

        memset(&reply, 0, sizeof(reply));

        int n = recvfrom(sockfd, &reply, sizeof(reply), 0, (struct sockaddr *)&targetAddr, &target_len);//Waiting for the subsequent SYN-ACK reply

        if(n > 0 && reply.header.flags == (SYN | ACK)){
            std::cout << "Received SYN-ACK. Sending final ACK...\n";
            Packet ack_pkt;
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.header.flags = ACK;
            
            sendto(sockfd, &ack_pkt, sizeof(Header), 0, (const struct sockaddr *)&targetAddr, sizeof(targetAddr));//Sending the final ACK response
            
            std::cout << "Connection ESTABLISHED!" << std::endl;
            established = true;
        }
        else{//Invoking Timeout if no Reply is received within the time limit
            std::cout<<"ACK Timeout! Retransmitting SYN request"<<std::endl;
            req_limit--;
        }
    }

    if(req_limit <= 0){//Ends the program if too many Timeouts occur
        std::cout<<"Request Limit Exceeded! Could not connect to server\n";
        return false;
    }

    return true;
}

void RUDPSocket::Attach(int socket_fd, struct sockaddr_in peer_addr){
    this->sockfd = socket_fd;
    this->targetAddr = peer_addr;
    this->target_len = sizeof(peer_addr);

    this->current_seq = 1;
    this->expected_seq = 1;

    setTimeout(1);

    std::cout<<"[RUDP Socket] Successfully attached socket\n";
}

bool RUDPSocket::Bind(int port){
    targetAddr.sin_port = htons(port); //Setting the port after converting it from host to network format

    if(bind(sockfd, (const struct sockaddr *)&targetAddr, sizeof(targetAddr)) < 0){// Binding the socket to the specific address and port
        perror("Bind Failed!\n");
        return false;
    }
    return true;
}

bool RUDPSocket::Accept(){
    std::cout<<"\nWaiting for Request"<<std::flush;
    Packet buffer;
    int n = 0;
    while(1){// Establishing a connection with the connection using a three way handshake
        n = recvfrom(sockfd, &buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&targetAddr, &target_len); // Waiting for request from client
        std::cout<<"."<<std::flush;
        if(n > 0){
            uint8_t flags = buffer.header.flags; //Reading the flags from the received packet

            if(flags == SYN){ //If the packet is a SYN packet then we send an ACK back
                std::cout<<"Received SYN. Sending SYN-ACK...\n";
                //std::this_thread::sleep_for(std::chrono::seconds(2));//Adding an artificial delay so the handshake process can be seen in real time
                Packet reply;
                memset(&reply, 0, sizeof(reply));

                reply.header.flags = SYN | ACK;

                sendto(sockfd, &reply, sizeof(Header), 0, (struct sockaddr *)&targetAddr, target_len);
            }
            else if(flags == ACK || flags == DATA){ // IF the packet is an ACK packet then we establish the connection
                std::cout<<"Received ACK. Connection Established\n";
                return true;
            }
        }
    }
}

int RUDPSocket::Send(const char* data, size_t length){
    Packet data_pkt;

    memset(&data_pkt, 0, sizeof(data_pkt));

    data_pkt.header.flags = DATA;
    data_pkt.header.seq_num = htonl(current_seq);
    
    memcpy(data_pkt.payload, data, length);

    size_t packet_size = sizeof(Header) + length;

    uint16_t checksum = calculate_checksum(&data_pkt, packet_size);

    data_pkt.header.checksum = checksum;

    bool ack_received = false;
    bool need_transmission = true;
    while(!ack_received){// Keeps retransmitting the packet untill the approprite ACK is received
        int n = 0;

        if(need_transmission){
            //std::cout<<"Sending sequence "<<current_seq<<std::endl; 
            sendto(sockfd, &data_pkt, packet_size, 0, (struct sockaddr *)&targetAddr, sizeof(targetAddr));//Sending the initial DATA packet
            need_transmission = false;
        }

        Packet ack_reply;
        n = recvfrom(sockfd, &ack_reply, sizeof(ack_reply), 0, (struct sockaddr *)&targetAddr, &target_len);//Waiting for the subsequene response

        if(n > 0){
            uint8_t flags = ack_reply.header.flags;
            uint32_t ack_num = ntohl(ack_reply.header.ack_num);

            if((flags & ACK) && ack_num == current_seq){
                ack_received = true;
                current_seq++;
            }
        }
        else{
            need_transmission = true;
        }
    }
    return 0;
}

int RUDPSocket::Receive(char* buffer, size_t max_len){
    Packet incoming;
    int copy_len = 0;
    bool received = false;
    while(!received){
        setTimeout(3600);
        int n = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr *)&targetAddr, &target_len);// Waintng for incoming packets
        setTimeout(1);
        if(n > 0){

            uint16_t validation = calculate_checksum(&incoming, n);
            if(validation != 0){
                continue;
            }

            uint8_t flags = incoming.header.flags;// Reading the flags for the packet
            uint32_t seq = ntohl(incoming.header.seq_num);// Reading the sequence number of the packet

            if(flags & DATA){

                if(seq == expected_seq){
                    int payload_len = n - sizeof(Header);
                    copy_len = (max_len > payload_len) ? payload_len : max_len;

                    memcpy(buffer, incoming.payload, copy_len);
                    expected_seq++;
                }

                Packet ack_pkt;

                memset(&ack_pkt, 0, sizeof(ack_pkt));
                ack_pkt.header.flags = ACK;
                ack_pkt.header.ack_num = htonl(seq);
                //std::cout<<"sending ack "<<seq<<std::endl;
                sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&targetAddr, target_len);// Sending the appropriate ack sequence back
                received = true;
            }
        }
    }    
    return copy_len;
}