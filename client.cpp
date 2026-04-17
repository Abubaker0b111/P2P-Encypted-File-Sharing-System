#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include<chrono>
#include<sodium.h>
#include"rudp_socket.h"

#define SERVER_IP "13.60.232.217" 
#define SERVER_PORT 9000

int main(int argc, char *argv[]){
    //initalizing Libsodium
    if (sodium_init() < 0) {
        std::cerr<<"Panic! Libsodium couldn't be initialized. It is not safe to use."<<std::endl;
        return 1;
    }
    std::cout<<"[System] Libsodium initialized successfully."<<std::endl;

    if(argc < 3){
        std::cout<<"Usage: ./client <my_name> <command> [target_name]"<<std::endl;
        return -1;
    }

    std::string my_name = argv[1];
    std::string command = argv[2];

    int sockfd;
    struct sockaddr_in serverAddr, peerAddr;
    char buffer[1024];

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket creation failed");
        return -1;
    }


    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    //Registering the Client With the rendezvous server
    std::string reg_msg = "REGISTER " + my_name;
    sendto(sockfd, reg_msg.c_str(), reg_msg.length(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    

    //Waiting for the response from the server
    socklen_t len = sizeof(serverAddr);
    int n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&serverAddr, &len);
    buffer[n] = '\0';
    std::cout << "[Server Response] " << buffer << std::endl;

    //Variables to hold the peer's info once we get it
    std::string peer_ip = "";
    int peer_port = 0;

    //Handle Caller vs Listener logic
    if(command == "CONNECT" && argc == 4){
        std::string target = argv[3];
        std::string conn_msg = "CONNECT " + target;
        sendto(sockfd, conn_msg.c_str(), conn_msg.length(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        
        //Wait for PEER_INFO from server
        n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&serverAddr, &len);
        buffer[n] = '\0';
        std::string response(buffer);
        
        if(response.find("PEER_INFO") != std::string::npos){
            //Parsing "PEER_INFO IP:PORT"
            size_t space_idx = response.find(' ');
            size_t colon_idx = response.find(':');
            peer_ip = response.substr(space_idx + 1, colon_idx - space_idx - 1);
            peer_port = std::stoi(response.substr(colon_idx + 1));
            std::cout << "[Info] Target located at " << peer_ip << ":" << peer_port << std::endl;
        }

    } 
    else if(command == "WAIT"){
        std::cout<<"[Info] Waiting for an incoming connection request..."<<std::endl;
        //Wait for INCOMING from server
        n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&serverAddr, &len);
        buffer[n] = '\0';
        std::string response(buffer);
        
        if(response.find("INCOMING") != std::string::npos){
            // Parsing "INCOMING IP:PORT"
            size_t space_idx = response.find(' ');
            size_t colon_idx = response.find(':');
            peer_ip = response.substr(space_idx + 1, colon_idx - space_idx - 1);
            peer_port = std::stoi(response.substr(colon_idx + 1));
            std::cout << "[Info] Incoming request from " << peer_ip << ":" << peer_port << std::endl;
        }
    }
    bool connected;
    if(peer_port != 0){
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){//Setting up the socket to timeout after the given time value
            perror("Error setting timeout\n");
        }

        //Punching Hole through the NAT
        std::cout<<"\nINITIATING UDP HOLE PUNCH" << std::endl;
        
        memset(&peerAddr, 0, sizeof(peerAddr));
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(peer_port);
        inet_pton(AF_INET, peer_ip.c_str(), &peerAddr.sin_addr);

        std::string punch_msg = "PUNCH_FROM_" + my_name;

        connected = false;
        int limit = 10;
        while(!connected && limit > 0){//Repeatedly tries to punch through NAt untill successful or till limit is excceded
            sendto(sockfd, punch_msg.c_str(), punch_msg.length(), 0, (struct sockaddr *)&peerAddr, sizeof(peerAddr));

            std::cout << "Punch sent! Waiting for peer..." << std::endl;
            socklen_t peer_len = sizeof(peerAddr);
            n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&peerAddr, &peer_len);
            
            if(n > 0){
                buffer[n] = '\0';
                std::cout<<" \nSUCCESS! DIRECT P2P ESTABLISHED!\n "<<std::endl;
                //std::cout<<" Received: " << buffer << std::endl;
                sendto(sockfd, punch_msg.c_str(), punch_msg.length(), 0, (struct sockaddr *)&peerAddr, sizeof(peerAddr));
                connected = true;
            }
            limit--;
        }
    }
    if(connected){
        RUDPSocket rudp_sock;
        //Now we update or raw socket to reliable udp socket
        rudp_sock.Attach(sockfd, peerAddr);

        std::cout<<"\n[Crypto] Initiating Elliptic-Curve Key Exchange..."<<std::endl;

        //Generate Keypair for this session
        unsigned char my_pk[crypto_kx_PUBLICKEYBYTES];//public key
        unsigned char my_sk[crypto_kx_SECRETKEYBYTES];//secret key
        crypto_kx_keypair(my_pk, my_sk);

        unsigned char peer_pk[crypto_kx_PUBLICKEYBYTES];//public key of the peer we want to communicate with
        memset(peer_pk , 0 , sizeof(peer_pk));

        //Exchanging Public Keys 
        if(command == "CONNECT"){
            std::cout<<"[Crypto] Sending my Public Key..."<<std::endl;
            rudp_sock.Send((const char*)my_pk, crypto_kx_PUBLICKEYBYTES);//Sending our public key
            
            std::cout<<"[Crypto] Waiting for Peer's Public Key..."<<std::endl;
            rudp_sock.Receive((char*)peer_pk, crypto_kx_PUBLICKEYBYTES);//Receiving the public key of the other peer
        } 
        else if(command == "WAIT"){
            std::cout<<"[Crypto] Waiting for Peer's Public Key..."<<std::endl;
            rudp_sock.Receive((char*)peer_pk, crypto_kx_PUBLICKEYBYTES);//Waiting for the caller's public key
            
            std::cout<<"[Crypto] Sending my Public Key..."<<std::endl;
            rudp_sock.Send((const char*)my_pk, crypto_kx_PUBLICKEYBYTES);//Sending our public key
        }

        //Generating the Session Keys
        unsigned char rx[crypto_kx_SESSIONKEYBYTES];
        unsigned char tx[crypto_kx_SESSIONKEYBYTES];

        if(command == "CONNECT"){
            //Client calculates keys
            if(crypto_kx_client_session_keys(rx, tx, my_pk, my_sk, peer_pk) != 0){
                std::cerr<<"[FATAL] Suspicious peer public key. Terminating."<<std::endl;
                return -1;
            }
        }
        else{
            //Server calculates keys
            if(crypto_kx_server_session_keys(rx, tx, my_pk, my_sk, peer_pk) != 0){
                std::cerr<<"[FATAL] Suspicious peer public key. Terminating."<<std::endl;
                return -1;
            }
        }

        std::cout<<"[Crypto] SUCCESS! Secure Session Keys generated."<<std::endl;
        rudp_sock.EnableEncryption(tx, rx);
        
        int i=1;
        if(command == "CONNECT"){
            for(int i=1 ; i<=5 ; i++){
                std::string msg = "A very secret message #" + std::to_string(i);
                rudp_sock.Send(msg.c_str(), msg.length());
            }
        }
        else{
            while(i<=5){
                char msg[1024];
                int length = rudp_sock.Receive(msg,1024);
                msg[length] = '\0';

                std::cout<<i<<": "<<msg<<std::endl;
                i++;
            }
        }
        
        //std::cout<<"My TX Key starts with: "<<std::hex<<(int)tx[0]<<(int)tx[1]<<(int)tx[2]<<std::dec<<std::endl;
        //std::cout<<"My RX Key starts with: "<<std::hex<<(int)rx[0]<<(int)rx[1]<<(int)rx[2]<<std::dec<<std::endl;

        
        //Wiping the keys from memory after use
        sodium_memzero(my_sk, sizeof(my_sk));
    }

    close(sockfd);
    return 0;
}