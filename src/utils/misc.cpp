#include<cerrno>
#include<cstring>
#include<iostream>

#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<arpa/inet.h>
#include <csignal>

    constexpr uint16_t  kPort       = 8080;
    constexpr int       kBacklog    = 16;
    constexpr size_t    kBufferSize = 4096;
    
    [[noreturn]] void die(const char* what){
        std::cerr << what << " failed: " << std::strerror(errno) << " (errno " << errno << ")\n";
        std::exit(1);
    }
    bool is_disconnect(int e) noexcept {
        return e == EPIPE || e == ECONNRESET || e == ENOTCONN || e == ETIMEDOUT;
    }

    void log_peer(const sockaddr_in&  client, int fd){
        char ip[INET_ADDRSTRLEN];
        if(::inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip)) == nullptr){
            std::strcpy(ip, "?");
        }
        std::cout << "connection from " << ip << ":" << ntohs(client.sin_port) << " (fd " << fd << ")\n";

    }
