#include "tyson/request_reader.hpp"

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

}