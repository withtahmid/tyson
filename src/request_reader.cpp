#include "tyson/request_reader.hpp"
#include "tyson/http.hpp"

#include <optional>

#include <charconv>

#include <unistd.h>

#include <cerrno>

#include "tyson/config.hpp"
#include "tyson/error.hpp"

namespace tyson {
namespace {
   
enum class FillOutcome {
    got_bytes,
    eof,
    disconnected,
    io_error
};

FillOutcome fill(int fd, std::string& buffer) {
    char chunk[kBufferSize];
    while(true) {
        const ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if(n > 0){
            buffer.append(chunk, static_cast<std::size_t>(n));
            return FillOutcome::got_bytes;
        }
        if(n == 0) return FillOutcome::eof;
        if(errno == EINTR) continue;
        if(is_disconnect(errno)) return FillOutcome::disconnected;
        return FillOutcome::io_error;
    }
}

[[nodiscard]] std::optional<std::size_t> parse_content_length(std::string_view text) {
    std::size_t value = 0;
    const char* begin = text.data();
    const char* end   = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if(ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

}

HeadResult read_head(int fd){
    HeadResult result;
    while(true){

        const std::size_t pos = result.buffer.find("\r\n\r\n");
        if(pos != std::string::npos){
            result.head_end = pos;
            result.outcome = ReadOutcome::ok;
            return result;
        }

        if(result.buffer.size() > kMaxHeadSize){
            result.outcome = ReadOutcome::head_too_large;
            return result;
        }

        switch(fill(fd, result.buffer)){
            case FillOutcome::got_bytes: 
                break;
            case FillOutcome::eof: 
                result.outcome = result.buffer.empty() 
                    ? ReadOutcome::disconnected 
                    : ReadOutcome::malformed;
                return result;
            case FillOutcome::disconnected: 
                result.outcome = ReadOutcome::disconnected;
                return result;
            case FillOutcome::io_error:
                result.outcome = ReadOutcome::io_error;
                return result;
        }
    }

}

ReadResult read_request(int fd) {
    ReadResult result;
    HeadResult head = read_head(fd);
    if(head.outcome != ReadOutcome::ok){
        result.outcome = head.outcome;
        return result;
    }

    const std::string_view head_view{head.buffer.data(), head.head_end};
    if(!http::parse_head(head_view, result.request)){
        result.outcome = ReadOutcome::malformed;
        return result;
    }

    std::size_t content_length = 0;
    if(const auto raw = http::find_header(result.request, "Content-Length")){
        const auto parsed = parse_content_length(*raw);
        if(!parsed){
            result.outcome = ReadOutcome::malformed;
            return result;
        }
        content_length = *parsed;
    }
    if(content_length > kMaxBodySize){
        result.outcome = ReadOutcome::body_too_large;
        return result;
    }


    const std::size_t body_start = head.head_end + 4;
    result.request.body = head.buffer.substr(body_start);

    while(result.request.body.size() < content_length){
        switch(fill(fd, result.request.body)){
            case FillOutcome::got_bytes:
                break;
            case FillOutcome::eof:
            case FillOutcome::disconnected:
                result.outcome = ReadOutcome::malformed;
                return result;
            case FillOutcome::io_error:
                result.outcome = ReadOutcome::io_error;
                return result;
        }
    }

    result.request.body.resize(content_length);
    result.outcome = ReadOutcome::ok;
    return result;
}

}