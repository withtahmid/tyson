#include "tyson/io.hpp"

#include <unistd.h>

#include <cerrno>

namespace tyson {

bool write_all(int fd, const char* data, std::size_t len){
    std::size_t sent = 0;
    while(sent < len) {
        const ssize_t n = ::write(fd, data + sent, len - sent);
        if(n < 0){
            if(errno == EINTR) continue;
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

}