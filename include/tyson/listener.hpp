#pragma once 

#include <cstdint>
#include "tyson/file_descriptor.hpp"
namespace tyson {
    [[nodiscard]] FileDescriptor make_listener(std::uint16_t port);
}