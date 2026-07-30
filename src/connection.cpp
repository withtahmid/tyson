#include "tyson/connection.hpp"
#include "tyson/request_reader.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "tyson/config.hpp"
#include "tyson/error.hpp"
#include "tyson/io.hpp"
#include "tyson/http.hpp"

namespace tyson {

std::string peer_to_string(const sockaddr_in& peer) {
    char ip[INET_ADDRSTRLEN];
    if(::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr){
        return "?:?";
    }
    return std::string{ip} + ":" + std::to_string(ntohs(peer.sin_port));
}

void handle_connection(int connection_fd) {
    const HeadResult head = read_head(connection_fd);

    switch(head.outcome){
        case ReadOutcome::ok: 
            std::cout << " head complete: " << head.head_end << " bytes before the blank line "
                      << head.buffer.size() << " bytes buffered total\n";
            break;
        case ReadOutcome::disconnected:
            std::cout << " peer left without a request\n";
            return;
        case ReadOutcome::malformed: 
            std::cout << " connection died mid-request\n";
            return;
        case ReadOutcome::head_too_large:
            std::cout << " head too large\n";
            return;
        case ReadOutcome::io_error:
            std::cerr << " read: " << std::strerror(errno) << "\n";
            return;
    }

    const std::string_view head_view{head.buffer.data(), head.head_end};
    const std::size_t line_end = head_view.find("\r\n");
    const std::string_view request_line = line_end == std::string::npos
                                            ? head_view
                                            : head_view.substr(0, line_end);

    http::Request request;
    if(http::parse_request_line(request_line, request)){
        std::cout << " method=" << request.method_text
                  << " target=" << request.target
                  << " version=" << request.version << "\n";
    }else{
        std::cout << " malformed request line\n";
    }


    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello World\n"; 

    if(!write_all(connection_fd, response.data(), response.size())){
        std::cerr << " write: " << std::strerror(errno) << "\n";
    }
}

}