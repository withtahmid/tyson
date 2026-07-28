#include "utils/die_with_error_log.cpp"
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<iostream>

int main(){
    
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0){
        die("socket");
    }

    sockaddr_in addr{};
    memset(&addr, 0, sizeof addr);

    addr.sin_family         = AF_INET;
    addr.sin_port           = htons(8080);
    addr.sin_addr.s_addr    = htonl(INADDR_ANY);

    auto bind_state = ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if(bind_state < 0){
        die("bind");
    }


    std::cout << "got fd " << listen_fd << "\n";

    if(::close(listen_fd) < 0){
        die("close");
    }
    
    return 0;
}