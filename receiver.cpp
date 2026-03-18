#include"rudp_socket.h"

int main(){
    RUDPSocket sck;
    char buffer[1024];

    if(sck.Bind(8080)) {sck.Accept();}
    
    while(buffer[0] != 'q'){
        sck.Receive(buffer, 1024);
        std::cout<<">"<<buffer<<"\n";
    }
}