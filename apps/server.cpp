#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <csignal>
#include <iostream>


#include "tyson/config.hpp"
#include "tyson/connection.hpp"
#include "tyson/error.hpp"
#include "tyson/file_descriptor.hpp"
#include "tyson/listener.hpp"


int main() {

    ::signal(SIGPIPE, SIG_IGN);

    const tyson::FileDescriptor listener = tyson::make_listener(tyson::kDefaultPort);

    std::cout << "listening on 0.0.0.0:" << tyson::kDefaultPort << " (fd " << listener.get() << ")\n";

    while(true) {
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        
        tyson::FileDescriptor connection {
            ::accept(
                listener.get(), reinterpret_cast<sockaddr*>(&peer), &peer_len
            )
        };

        if(!connection.valid()){
            
            if(errno == EINTR) continue;
            if(errno == ECONNABORTED) continue;

            tyson::die("accept");

        }

        std::cout << "connection from " << tyson::peer_to_string(peer) << " (fd " << connection.get() << ")\n";

        tyson::handle_connection(connection.get());
        std::cout << "connection closed\n";

    }

    return 0;
}