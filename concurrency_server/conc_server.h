#pragma once

#include <sys/epoll.h>

#include "base_server/base_server.h"
#include "rate_limiter/thread_pool.h"

constexpr unsigned MAXFDS = 16 * 1024;
constexpr unsigned SENDBUF_SIZE = 1024;

enum class FdStatus {
    RECV,
    SEND,
    DISCONNECTED,
};

struct PeerState {
    ProcessingState state;
    uint8_t sendbuf[SENDBUF_SIZE];
    int sendbuf_end;
    int sendptr;
    FdStatus status;
};

class ConcServer : public BaseServer {

public:
    using BaseServer::BaseServer;
    ConcServer(int port, int num_threads);
    int Run() override;

private:
    int MakeSocketNonBlocking(int fd);
    int ConnectPeer(int sockfd);
    int PeerSend(int sockfd);
    int PeerRecv(int sockfd);

    std::unique_ptr<ThreadPool> thread_pool;

    struct epoll_event events[MAXFDS];

    // Each peer is globally identified by the file descriptor (fd) it's connected
    // on. As long as the peer is connected, the fd is unique to it. When a peer
    // disconnects, a new peer may connect and get the same fd. on_peer_connected
    // should initialize the state properly to remove any trace of the old peer on
    // the same fd.
    PeerState global_state[MAXFDS];
};