#pragma once

#include <string>

namespace tyson {

enum class ReadOutcome {
    ok,
    disconnected,
    malformed,
    head_too_large,
    io_error
};

struct HeadResult {
    ReadOutcome outcome = ReadOutcome::io_error;
    std::string buffer;
    std::size_t head_end = 0;
};

[[nodiscard]] HeadResult read_head(int fd);

}