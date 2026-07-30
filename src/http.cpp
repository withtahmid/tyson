#include "tyson/http.hpp"

namespace tyson::http {

Method method_from_text(std::string_view text) noexcept {
    if(text == "GET")   return Method::get;
    if(text == "HEAD")  return Method::head;
    if(text == "POST")  return Method::post;
    return Method::unknown;
}

bool parse_request_line(std::string_view line, Request& out){
    const std::size_t first_space = line.find(' ');
    if(first_space == std::string_view::npos) return false;


    const std::size_t second_space = line.find(' ', first_space + 1);
    if(second_space == std::string_view::npos) return false;

    if(line.find(' ', second_space + 1) != std::string::npos) return false;

    const std::string_view method_text  = line.substr(0, first_space);
    const std::string_view target       = line.substr(first_space + 1, second_space - first_space - 1);
    const std::string_view version      = line.substr(second_space + 1);

    if(method_text.empty() || target.empty()) return false;

    if(target.front() != '/' && target != "*") return false;

    if(version != "HTTP/1.1" && version != "HTTP/1.0") return false;

    out.method_text = std::string{method_text};
    out.method      = method_from_text(method_text);
    out.target      = std::string{target};
    out.version     = std::string{version};

    return true;

}

}