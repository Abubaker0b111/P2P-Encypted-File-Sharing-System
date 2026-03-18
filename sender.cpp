#include"rudp_socket.h"


int main(){
    RUDPSocket sck;
    std::string input;

    if(sck.Connect("127.0.0.1", 8080)){
        while(input[0] != 'q'){
            std::cout<<"Enter a message: ";
            std::getline(std::cin, input);
            size_t len = input.length();
            const char* cinput = input.c_str();
            sck.Send(cinput, len);
        }
    };  
    
}