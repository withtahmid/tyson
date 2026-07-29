
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<iostream>
#include<arpa/inet.h>
#include <csignal>
FileDescriptor make_listener(uint16_t port){
    FileDescriptor fd = FileDescriptor(::socket(AF_INET, SOCK_STREAM, 0));
    
    if(!fd.valid()) {
        die("socket");
    }

    int yes = 1;
    if(::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
        die("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family         = AF_INET;
    addr.sin_port           = htons(port);
    addr.sin_addr.s_addr     = htonl(INADDR_ANY);

    if(::bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        die("bind");
    }

    if(::listen(fd.get(), kBacklog) < 0){
        die("listen");
    }

    return fd;

} 