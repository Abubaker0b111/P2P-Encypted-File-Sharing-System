#ifndef RUDP_SOCKET_H
#define RUDP_SOCKET_H

#include<iostream>
#include<cstring>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<cstdio>
#include<cstdint>
#include"packet.h"
#include<sodium.h>


class RUDPSocket{
    private:
        int sockfd;
        bool own_sock;
        struct sockaddr_in targetAddr;
        socklen_t target_len;

        uint32_t current_seq;
        uint32_t expected_seq;

        unsigned char tx_key[crypto_kx_SESSIONKEYBYTES];//transmission key
        unsigned char rx_key[crypto_kx_SESSIONKEYBYTES];//receiving key
        bool secure_mode = false;

        uint16_t calculate_checksum(void* vdata, size_t length);
        void setTimeout(int seconds);
    public:
        RUDPSocket();
        ~RUDPSocket();

        //Binds the socket to the provided port
        bool Bind(int port);
        //Accepts incomming connection requests
        bool Accept();
        //Establish reliable connection to a peer
        bool Connect(const std::string& ip, int port);
        //Attach the reliable socket to a previously created socket
        void Attach(int socket_fd, struct sockaddr_in peer_addr);
        //Sends data
        int Send(const char* data, size_t length);
        //Receives data
        int Receive(char* buffer, size_t max_len);
        //It's in the name
        void EnableEncryption(const unsigned char* tx, const unsigned char* rx);
};

#endif