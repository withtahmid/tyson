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

    const ssize_t n = ::read(connection_fd, buf, sizeof(buf));
    if(n <= 0){
        std::cout << " peer left before sending anything useful\n";
        return;
    }
    std::cout << " read " << n << " bytes or request (ignored)\n";

    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello world\n"; 

    if(!write_all(connection_fd, response.data(), response.size())){
        std::cerr << " write: " << std::strerror(errno) << "\n";
    }
}

}