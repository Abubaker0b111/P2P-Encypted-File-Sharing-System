#include<iostream>
#include<cstring>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

int main(){
    int sockfd;
    struct sockaddr_in server_addr, client_addr;

    const char* hello = "Hello this is a msg from sender!\n";

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

    sendto(sockfd, hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr*)&server_addr, sizeof(server_addr));

    std::cout<<"MSG Sent.\n";

    close(sockfd);

    return 0;
}