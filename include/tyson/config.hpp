#pragma once

#include <cstddef>
#include <cstdint>

namespace tyson {
    inline constexpr std::uint16_t  kDefaultPort    = 8080;
    inline constexpr int            kBacklog        = 16;
    inline constexpr std::size_t    kBufferSize     = 4096;
    inline constexpr std::size_t    kMaxHeadSize    = 8 * 1024;
    inline constexpr std::size_t    kMaxBodySize    = 1024 * 1024;
}