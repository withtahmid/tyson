#include "tyson/http.hpp";

namespace tyson::http {

Method method_from_text(std::string_view text) noexcept {
    if(text == "GET")   return Method::get;
    if(text == "HEAD")  return Method::head;
    if(text == "POST")  return Method::post;
    return Method::unknown;
}

bool parse_request_line(std::string_view, Request& out){
    
}

}