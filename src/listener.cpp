#include "tyson/listener.hpp"

#include <netinet/in.h>
#include <sys/socket.h>

#include "tyson/config.hpp"
#include "tyson/error.hpp"
#include "tyson/file_descriptor.hpp"

namespace tyson {

FileDescriptor make_listener(std::uint16_t port) {
    FileDescriptor fd{
        ::socket(AF_INET, SOCK_STREAM, 0)
    };

    if(!fd.valid()) die("socket");

    const int enable = 1;
    if(::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0){
        die("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(::bind(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        die("bind");
    }

    if(::listen(fd.get(), kBacklog) < 0){
        die("listen");
    }

    return fd;
}

}