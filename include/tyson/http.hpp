#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tyson::http {

enum class Method { get, head, post, unknown };

struct Header {
    std::string name;
    std::string value;
};

struct Request {
    Method              method      = Method::unknown;
    std::string         method_text;
    std::string         target;
    std::string         version;
    std::vector<Header> headers;
    std::string         body;
};

[[nodiscard]] Method method_from_text(std::string_view text) noexcept;

[[nodiscard]] bool parse_request_line(std::string_view line, Request& out);


}