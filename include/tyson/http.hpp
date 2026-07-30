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

struct Response {
    int status = 200;
    std::vector<Header> headers;
    std::string body;
};

[[nodiscard]] Method method_from_text(std::string_view text) noexcept;

[[nodiscard]] bool parse_request_line(std::string_view line, Request& out);

[[nodiscard]] bool parse_header_line(std::string_view line, Request& out);

[[nodiscard]] bool parse_head(std::string_view head, Request& out);

[[nodiscard]] std::optional<std::string_view>
            find_header(const Request& request, std::string_view name) noexcept;

[[nodiscard]] std::string_view reason_phrase(int status) noexcept;
[[nodiscard]] std::string serialize(const Response& response);

Response make_error_response(int status);
Response route (const Request& Request);

}