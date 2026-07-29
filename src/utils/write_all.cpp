bool write_all(int fd, const char* data, size_t len){
    size_t sent = 0;
    while(sent < len){
        ssize_t n = ::write(fd, data + sent, len - sent);
        if(n < 0){
            if(errno == EINTR){
                continue;
            }
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}