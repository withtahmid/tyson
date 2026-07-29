#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<iostream>
#include<arpa/inet.h>
#include <csignal>

void handle_connection(int conn_fd) {
    char buf[kBufferSize];

    while(true){
        ssize_t n = ::read(conn_fd, buf, sizeof(buf));
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

        if(!write_all(conn_fd, buf, static_cast<size_t>(n))) {
            if(is_disconnect(errno)) {
                std::cout << " peer gone mid-write: " << std::strerror(errno) << "\n";
                return;
            }
            std::cerr << " write: " << std::strerror(errno) << "\n";
            return;
        }
    }

}