#pragma once

#include "base_server/base_server.h"

class conc_server : public base_server {

public:
    using base_server::base_server;
    int Run() override;
};