#pragma once

#include "base_server/base_server.h"

class SeqServer: public BaseServer {

public:
    using BaseServer::BaseServer;
    int Run() override;
};