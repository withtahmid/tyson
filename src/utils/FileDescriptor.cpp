
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<iostream>
#include<arpa/inet.h>
#include <csignal>


class FileDescriptor{
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd){}
    ~FileDescriptor(){
        reset();
    }

    FileDescriptor(const FileDescriptor&)       = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& o) noexcept: fd_(o.fd_) {
        o.fd_ = -1;
    }
    FileDescriptor& operator=(FileDescriptor&& o) noexcept{
        if(this != &o){
            reset();
            fd_ = o.fd_;
            o.fd_ = -1;
        }
        return *this;
    }

    int get()   const noexcept{
        return fd_;
    }
    bool valid() const noexcept{
        return fd_ >= 0;
    }
    void reset() noexcept{
        if(fd_ >= 0){
            ::close(fd_);
            fd_ = -1;
        }
    }
private:
    int fd_;

};