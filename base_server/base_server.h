#pragma once

#include <sys/types.h>
#include <netinet/in.h>

enum class ProcessingState { 
    WAIT_FOR_MSG, 
    IN_MSG,
    INITIAL_ACK,
};

class BaseServer {

public:
    BaseServer(int port) : port(port) {};
    virtual int Run();

protected:
    int port;
    int socket_fd;
    int ListenInetSocket(int port_num);
    int ServeConnection(int new_sock_fd);
    // void report_peer_connected(sockaddr_in &peer_addr, socklen_t peer_addr_len);

};