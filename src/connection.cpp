#include "tyson/connection.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "tyson/config.hpp"
#include "tyson/error.hpp"
#include "tyson/io.hpp"

namespace tyson {

std::string peer_to_string(const sockaddr_in& peer) {
    char ip[INET_ADDRSTRLEN];
    if(::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr){
        return "?:?";
    }
    return std::string{ip} + ":" + std::to_string(ntohs(peer.sin_port));
}

void handle_connection(int connection_fd) {
    char buf[kBufferSize];

    while(true) {
        ssize_t n = ::read(connection_fd, buf, sizeof(buf));

        if(n == 0){
            std::cout << " peer closed (EOF)\n";
            return;
        }
        
        if(n < 0){
            if(errno == EINTR) continue;
            if(is_disconnect(errno)){
                std::cout << " peer gone: " << std::strerror(errno) << "\n";
                return;
            }
            std::cerr << " read: " << std::strerror(errno) << "\n";
            return;
        }
        std::cout  << " read " << n << " bytes\n";
        
        if(!write_all(connection_fd, buf, static_cast<std::size_t>(n))){
            if(is_disconnect(errno)){
                std::cout << " peer gone mid write: " << std::strerror(errno) << "\n";
                return;
            }
            std::cerr << " write: " << std::strerror(errno) << "\n"; 
            return;
        }
    
    }

}

}