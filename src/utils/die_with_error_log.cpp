#include<cerrno>
#include<cstring>
#include<iostream>

[[noreturn]] void die(const char* what){
    std::cerr << what << " failed: " << std::strerror(errno) << " (errno " << errno << ")\n";
    std::exit(1);
}