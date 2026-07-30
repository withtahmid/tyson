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
    const ReadResult result = tyson::read_request(connection_fd);

    switch(result.outcome){
        case ReadOutcome::ok: 
            std::cout << " request complete: " << result.request.body.size() << " bytes of body\n";
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
        case ReadOutcome::body_too_large:
            std::cout << " body too large\n";
            return;
        case ReadOutcome::io_error:
            std::cerr << " read: " << std::strerror(errno) << "\n";
            return;
    }

    http::Request request = result.request;

  const std::optional<std::string_view> user_agent = find_header(request, "user-agent");

    std::cout <<
        "\n--- REQUEST DETAILS ----\n"
        "  Method=" << request.method_text << "\n"
        "  Target=" << request.target << "\n"
        "  Version=" << request.version << "\n"
        "  Header Count=" << request.headers.size() << "\n"
        "  Headers:\n"
        ;
        for(const http::Header&  header : request.headers){
            std::cout << "    " << header.name << ": " << header.value << "\n";
        }

        std::cout <<
        "  End of headers\n"
        "  Body length=" << request.body.size() << "\n"
        "  Body:\n    " << request.body<<  "\n"
        "  User-Agent=" << (user_agent ? *user_agent : std::string_view{"(absent)"}) << "\n"
        "---- END OF REQUEST -----\n";
        // "---- END OF REQUEST -----\n";




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