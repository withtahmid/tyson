#pragma once

#include <cstddef>
#include <cstdint>

namespace tyson {
    inline constexpr std::uint16_t  kDefaultPort    = 8080;
    inline constexpr int            kBacklog        = 16;
    inline constexpr std::size_t    kBufferSize     = 4096;
}