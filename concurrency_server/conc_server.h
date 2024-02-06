#pragma once

#include "base_server/base_server.h"
#include "rate_limiter/thread_pool.h"

class ConcServer : public BaseServer {

public:
    using BaseServer::BaseServer;
    ConcServer(int port, int num_threads);
    int Run() override;

private:
    std::unique_ptr<ThreadPool> thread_pool;
};