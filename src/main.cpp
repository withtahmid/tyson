
#include "utils/FileDescriptor.cpp"
#include "utils/misc.cpp"
#include "utils/write_all.cpp"
#include "utils/make_listener.cpp"
#include "utils/handle_connection.cpp"

#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cerrno>
#include<iostream>
#include<arpa/inet.h>
#include <csignal>

int main(){

    ::signal(SIGPIPE, SIG_IGN);

    FileDescriptor listener = make_listener(kPort);
    std::cout << "listening on 0.0.0.0:" << kPort << " (fd " << listener.get() << ")\n";

    while(true){
        sockaddr_in client{};
        memset(&client, 0, sizeof(client));
        socklen_t client_len = sizeof(client);

        FileDescriptor connection{
            ::accept(
                listener.get(),
                reinterpret_cast<sockaddr*>(&client),
                &client_len
            )
        };

        if(!connection.valid()){
            if(errno == EINTR){
                continue;
            }
            if(errno == ECONNABORTED){
                continue;
            }
            die("accept");
        }

        log_peer(client, connection.get());
        handle_connection(connection.get());
        std::cout << "connection closed\n";
    }
}