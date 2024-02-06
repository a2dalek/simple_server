#include "concurrency_server/conc_server.h"
#include <thread>
#include <sys/socket.h>
#include <iostream>
#include <functional>

ConcServer::ConcServer(int port, int num_threads) : BaseServer(port) {
    thread_pool = std::make_unique<ThreadPool>(num_threads);
}

int ConcServer::Run() {
    setvbuf(stdout, NULL, _IONBF, 0);
    std::cout << "Serving on port \n" << port;

    int socket_fd = ListenInetSocket(port);
    while (true)
    {
        struct sockaddr_in peer_addr;
        socklen_t len_peer_addr = sizeof(peer_addr);

        int new_fd = accept(socket_fd, (struct sockaddr*)&peer_addr, &len_peer_addr);
        
        if (thread_pool) {
            thread_pool->submit_job([this, new_fd] {
                this->ServeConnection(new_fd);
            });
        } else {
            std::thread t([this, new_fd]() {
                this->ServeConnection(new_fd);            
            });
            
            t.detach();
        }
    }  
}