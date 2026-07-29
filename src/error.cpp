#include "tyson/error.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace tyson {

void die(const char* what){
    const int err= errno;

    std::cerr << "fatal: " << what << ": " << std::strerror(err) << " (errno " << err << ")\n";
    std::exit(EXIT_FAILURE);

}

bool is_disconnect(int err) noexcept {
  return err == EPIPE || err == ECONNRESET || err == ENOTCONN ||
         err == ETIMEDOUT;
}

}