#include"rudp_socket.h"

RUDPSocket::RUDPSocket(){
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket Creation Error!\n");
        return;
    }
    own_sock = true;
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET; //Setting the ip address type as ipv4
    targetAddr.sin_addr.s_addr = INADDR_ANY; //Setting the input so that the server listens for input from all available channels

    target_len = sizeof(targetAddr);
    current_seq = 1;
    expected_seq = 1;
    setTimeout(1);
}

RUDPSocket::~RUDPSocket(){
    if(own_sock){
        close(sockfd);
    }
}

//Divides the entire data into 16 bit chunks and adds them to calculate checksum
uint16_t RUDPSocket::calculate_checksum(void* vdata, size_t length){
    uint8_t* data = (uint8_t*)vdata;
    uint32_t acc = 0;

    //Adding 1 16 bit word at a time
    for(size_t i = 0; i + 1 < length; i += 2){
        uint16_t word;
        memcpy(&word, data + i, 2);
        acc += ntohs(word);
    }

    // Safely handle the leftover odd byte
    if(length % 2 != 0) {
        uint16_t word = 0;
        word = data[length - 1] << 8; 
        acc += word;
    }

    //Compressing the result to 16 bit
    while(acc >> 16){
        acc = (acc & 0xFFFF) + (acc >> 16);
    }

    return htons(~acc);
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

//Connect to the provided IP:Port if available
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

    int req_limit = 10; // Request Limit

    while(!established && req_limit > 0){// Trying to establish a three way handshake before communicating

        //std::cout<<"Sending SYN...\n";

        sendto(sockfd, &syn_pkt, sizeof(Header), 0, (const struct sockaddr *)&targetAddr, sizeof(targetAddr));//Sending the initial SYN packet

        Packet reply;

        memset(&reply, 0, sizeof(reply));

        int n = recvfrom(sockfd, &reply, sizeof(reply), 0, (struct sockaddr *)&targetAddr, &target_len);//Waiting for the subsequent SYN-ACK reply

        if(n > 0 && reply.header.flags == (SYN | ACK)){
            //std::cout << "Received SYN-ACK. Sending final ACK...\n";
            Packet ack_pkt;
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.header.flags = ACK;
            
            sendto(sockfd, &ack_pkt, sizeof(Header), 0, (const struct sockaddr *)&targetAddr, sizeof(targetAddr));//Sending the final ACK response
            
            //std::cout << "Connection ESTABLISHED!" << std::endl;
            established = true;
        }
        else{//Invoking Timeout if no Reply is received within the time limit
            //std::cout<<"ACK Timeout! Retransmitting SYN request"<<std::endl;
            req_limit--;
        }
    }

    if(req_limit <= 0){//Ends the program if too many Timeouts occur
        //std::cout<<"Request Limit Exceeded! Could not connect to server\n";
        return false;
    }

    return true;
}

//Attaches a pre Existing socket to the given address and port
void RUDPSocket::Attach(int socket_fd, struct sockaddr_in peer_addr){
    this->sockfd = socket_fd;
    this->targetAddr = peer_addr;
    this->target_len = sizeof(peer_addr);
    this->own_sock = false;

    this->current_seq = 1;
    this->expected_seq = 1;

    setTimeout(1);

    //std::cout<<"[RUDP Socket] Successfully attached socket\n";
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
    //std::cout<<"\nWaiting for Request"<<std::flush;
    Packet buffer;
    int n = 0;
    while(1){// Establishing a connection with the connection using a three way handshake
        n = recvfrom(sockfd, &buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&targetAddr, &target_len); // Waiting for request from client
        std::cout<<"."<<std::flush;
        if(n > 0){
            uint8_t flags = buffer.header.flags; //Reading the flags from the received packet

            if(flags == SYN){ //If the packet is a SYN packet then we send an ACK back
                //std::cout<<"Received SYN. Sending SYN-ACK...\n";
                //std::this_thread::sleep_for(std::chrono::seconds(2));//Adding an artificial delay so the handshake process can be seen in real time
                Packet reply;
                memset(&reply, 0, sizeof(reply));

                reply.header.flags = SYN | ACK;

                sendto(sockfd, &reply, sizeof(Header), 0, (struct sockaddr *)&targetAddr, target_len);
            }
            else if(flags == ACK || flags == DATA){ // IF the packet is an ACK packet then we establish the connection
                //std::cout<<"Received ACK. Connection Established\n";
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
    data_pkt.header.checksum = 0;
    size_t payload_len = length;

    if(secure_mode){
        //Generate a random 24-byte Nonce
        unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
        randombytes_buf(nonce, sizeof(nonce));
        
        //Prepare the Associated Data (The Header)
        unsigned char* ad = (unsigned char*)&data_pkt.header;
        unsigned long long ad_len = sizeof(Header);

        //Encrypt! (Ciphertext goes into the payload buffer AFTER the 24-byte nonce space)
        unsigned long long ciphertext_len;
        crypto_aead_xchacha20poly1305_ietf_encrypt(
            (unsigned char*)data_pkt.payload + sizeof(nonce), &ciphertext_len, // Destination
            (const unsigned char*)data, length,                                // Source
            ad, ad_len,                                                        // Associated Data
            NULL, nonce, tx_key                                                // Keys & Nonce
            );

        //Prepend the Nonce so the receiver knows what it is
        memcpy(data_pkt.payload, nonce, sizeof(nonce));

        //Update the length (Original + 24 byte Nonce + 16 byte MAC tag)
        payload_len = sizeof(nonce) + ciphertext_len;
    }
    else{
        memcpy(data_pkt.payload, data, length);
    }

    size_t packet_size = sizeof(Header) + payload_len;

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
    memset(&incoming, 0, sizeof(incoming));
    int copy_len = 0;
    bool received = false;
    while(!received){
        setTimeout(3600);
        int n = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr *)&targetAddr, &target_len);// Waintng for incoming packets
        setTimeout(1);
        if(n > 0){

            uint16_t validation = calculate_checksum(&incoming, n);
            if(validation != 0){
                std::cout<<"Checksum Failed\n";
                continue;
            }

            incoming.header.checksum = 0;

            uint8_t flags = incoming.header.flags;// Reading the flags for the packet
            uint32_t seq = ntohl(incoming.header.seq_num);// Reading the sequence number of the packet

            if(flags & DATA){

                if(seq == expected_seq){
                    int payload_len = n - sizeof(Header);

                    if(secure_mode){
                        //Extract the Nonce (first 24 bytes of the payload)
                        unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
                        memcpy(nonce, incoming.payload, sizeof(nonce));

                        //Identify the Ciphertext
                        unsigned char* ciphertext = (unsigned char*)incoming.payload + sizeof(nonce);
                        unsigned long long ciphertext_len = payload_len - sizeof(nonce);

                        //Prepare the Associated Data (The Header)
                        unsigned char* ad = (unsigned char*)&incoming.header;
                        unsigned long long ad_len = sizeof(Header);

                        //Decrypt!
                        unsigned long long decrypted_len;
                        if(crypto_aead_xchacha20poly1305_ietf_decrypt(
                            (unsigned char*)buffer, &decrypted_len,  // Destination buffer
                            NULL,
                            ciphertext, ciphertext_len,              // Source
                            ad, ad_len,                              // Associated Data
                            nonce, rx_key                            // Keys & Nonce
                            ) != 0){
                            std::cout << "[Crypto] ALERT: Forged or corrupted packet dropped!" << std::endl;
                            continue; // Drop packet, don't send ACK!
                        }
                        
                        copy_len = decrypted_len; // Set the output length for the user
                    }
                    else{
                        copy_len = (max_len > payload_len) ? payload_len : max_len;
                        memcpy(buffer, incoming.payload, copy_len);
                    }

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

void RUDPSocket::EnableEncryption(const unsigned char* tx, const unsigned char* rx){
    memcpy(this->tx_key, tx, crypto_kx_SESSIONKEYBYTES);
    memcpy(this->rx_key, rx, crypto_kx_SESSIONKEYBYTES);
    this->secure_mode = true;
    //std::cout << "[RUDP] Secure Mode Enabled. All traffic is now encrypted." << std::endl;
}
