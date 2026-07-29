#pragma once

#include <netinet/in.h>

#include <string>

namespace tyson {

    [[nodiscard]] std::string peer_to_string(const sockaddr_in& peer);

    void handle_connection(int connection_fd);   
}