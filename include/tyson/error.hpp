#pragma once 

namespace tyson {
    
    [[noreturn]] void die(const char* what);

    [[nodiscard]] bool is_disconnect(int err) noexcept;
}