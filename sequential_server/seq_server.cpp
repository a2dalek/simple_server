#include "sequential_server/seq_server.h"
#include <sys/socket.h>
#include <iostream>

int SeqServer::Run() {
    setvbuf(stdout, NULL, _IONBF, 0);
    std::cout << "Serving on port " << port << std::endl;

    socket_fd = ListenInetSocket(port);
    
    while (true) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        int new_sock_fd = accept(socket_fd, (struct sockaddr*)&peer_addr, &peer_addr_len);

        if (new_sock_fd < 0) {
            printf("Fail to accept new connect");
            return -1;
        }

        ServeConnection(new_sock_fd);
        printf("peer done\n");
    }
}