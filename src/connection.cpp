#include "tyson/connection.hpp"
#include "tyson/request_reader.hpp"

#include <arpa/inet.h>


#include <cerrno>
#include <cstring>
#include <iostream>


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

    http::Response response;
    switch (result.outcome) {
        case ReadOutcome::ok:
            std::cout << ' ' << result.request.method_text << ' '
                      << result.request.target << '\n';
            response = route(result.request);
            break;
        case ReadOutcome::malformed:
            std::cout << " malformed request -> 400\n";
            response = http::make_error_response(400);
            break;
        case ReadOutcome::head_too_large:
            std::cout << " oversized head -> 431\n";
            response = http::make_error_response(431);
            break;
        case ReadOutcome::body_too_large:
            std::cout << " oversized body -> 413\n";
            response = http::make_error_response(413);
            break;
        case ReadOutcome::disconnected:
            std::cout << " peer left without completing a request\n";
            return;
        case ReadOutcome::io_error:
            std::cerr << " read: " << std::strerror(errno) << '\n';
            return;
    }

    const std::string wire = http::serialize(response);
    if(!write_all(connection_fd, wire.data(), wire.size())){
        if (is_disconnect(errno)) {
            std::cout << " peer gone mid-response: " << std::strerror(errno) << '\n';
        } else {
            std::cerr << " write: " << std::strerror(errno) << '\n';
        }
    }
    std::cout << " responded " << response.status << '\n';
}

}