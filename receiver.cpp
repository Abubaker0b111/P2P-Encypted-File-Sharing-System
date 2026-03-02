#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(){
    int sockfd; //socket file descriptor
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket Creation Failed!\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind Failed!\n");
        return -1;
    }

    std::cout<<"Receiver is listening on Port "<<PORT<<"..."<<std::endl;
    int n = 0;
    while(1){

        socklen_t len = sizeof(client_addr);

        n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
        buffer[n] = '\0';
        if(buffer[0] == 'q') break;
        std::cout<<"Received: "<< buffer << std::endl;
    }
    close(sockfd);
    return 0;
}
