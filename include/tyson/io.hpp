#pragma once

#include <cstddef>

namespace tyson {
    [[nodiscard]] bool write_all(int fd, const char* data, std::size_t len);
}