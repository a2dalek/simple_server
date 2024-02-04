#include "concurrency_server/conc_server.h"
#include <thread>
#include <sys/socket.h>
#include <iostream>
#include <functional>

int conc_server::Run() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Serving on port %d\n", port);

    int socket_fd = ListenInetSocket(port);
    while (true)
    {
        struct sockaddr_in peer_addr;
        socklen_t len_peer_addr = sizeof(peer_addr);

        int new_fd = accept(socket_fd, (struct sockaddr*)&peer_addr, &len_peer_addr);
        
        std::thread t([this, new_fd]() {
            this->ServeConnection(new_fd);            
        });
        
        t.detach();
    }
    
}