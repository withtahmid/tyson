#include "tyson/http.hpp"
#include <iostream>
namespace tyson::http {

namespace {

[[nodiscard]] char to_lower_ascii (char c) noexcept {
    if(c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return c;
}

[[nodiscard]] bool equals_ignore_case(std::string_view a, std::string_view b) noexcept {
    if(a.size() != b.size()) return false;
    for(std::size_t i = 0; i < a.size(); ++i) {
        if(to_lower_ascii(a[i]) != to_lower_ascii(b[i])) return false;
    }
    return true;
}

[[nodiscard]] std::string_view trim_ows(std::string_view s) noexcept {
    while(!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while(!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

}

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


bool parse_header_line(std::string_view line, Request& out) {
    const std::size_t colon = line.find(':');
    if(colon == std::string_view::npos) return false;
    const std::string_view name = line.substr(0, colon);
    if(name.empty()) return false;

    for (const char c : name) {
        if(c == ' ' || c == '\t') return false;
    }

    const std::string_view value = trim_ows(line.substr(colon + 1));
    out.headers.push_back(Header{std::string{name}, std::string{value}});
    return true;
}

bool parse_head(std::string_view head, Request& out) {
    
    std::size_t line_end = head.find("\r\n");
    const std::string_view request_line = line_end == std::string_view::npos 
                                            ? head
                                            : head.substr(0, line_end);
    
    if(!parse_request_line(request_line, out)) return false;
    
    std::size_t pos = line_end == std::string_view::npos
                                    ? head.size()
                                    : line_end + 2;

    while(pos < head.size()) {

        const std::size_t next = head.find("\r\n", pos);
        const std::string_view line = next == std::string_view::npos 
                                                ? head.substr(pos)
                                                : head.substr(pos, next - pos);

        if(line.empty()) return false;
        
        if(!parse_header_line(line, out)) return false;
        
        pos = next == std::string_view::npos ? head.size() : next + 2;
    }
    return true;
}

std::optional<std::string_view>
find_header(const Request& request, std::string_view name) noexcept {
    for(const Header& header : request.headers){
        if(equals_ignore_case(header.name, name)){
            return std::string_view{header.value};
        }
    }
    return std::nullopt;
}

std::string_view reason_phrase(int status) noexcept {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 431: return "Request Header Fields Too Large";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

std::string serialize(const Response& response) {
    std::string out;
    out += "HTTP/1.1 ";
    out += std::to_string(response.status);
    out += ' ';
    out += reason_phrase(response.status);
    out += "\r\n";

    for (const Header& header : response.headers) {
        out += header.name;
        out += ": ";
        out += header.value;
        out += "\r\n";
    }

    out += "Content-Length: ";
    out += std::to_string(response.body.size());
    out += "\r\nConnection: close\r\n\r\n";

    out += response.body;

    return out;
}

http::Response make_error_response(int status) {
    http::Response response;
    response.status = status;
    response.headers.push_back({"Content-Type", "text/plain; charset=utf-8"});
    response.body = std::to_string(status);
    response.body += ' ';
    response.body += http::reason_phrase(status);
    response.body += '\n';
    return response;
}

http::Response route(const http::Request& request) {
    if(request.method == http::Method::unknown){
        return make_error_response(501);
    }
    if(request.method != http::Method::get){
        http::Response response = make_error_response(405);
        response.headers.push_back({"Allow", "GET"});
        return response;
    }
    if(request.target == "/"){
        http::Response response;
        response.headers.push_back({"Content-Type", "text/html; charset=utf-8"});
        response.body = 
            "<!doctype html>\n"
            "<html><head><title>tyson</title></head>\n"
            "<body><h1>tyson</h1><p>Handwritten HTTP, stage 2 complete.</p></body></html>\n";
        return response;
    }
    return make_error_response(404);
}

}