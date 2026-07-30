#pragma once

#include <string>
#include <cstddef>

#include "tyson/http.hpp"
namespace tyson {

enum class ReadOutcome {
    ok,
    disconnected,
    malformed,
    head_too_large,
    body_too_large,
    io_error
};

struct HeadResult {
    ReadOutcome outcome = ReadOutcome::io_error;
    std::string buffer;
    std::size_t head_end = 0;
};

struct ReadResult {
    ReadOutcome outcome = ReadOutcome::io_error;
    http::Request request;
};

[[nodiscard]] HeadResult read_head(int fd);

[[nodiscard]] ReadResult read_request(int fd);

}