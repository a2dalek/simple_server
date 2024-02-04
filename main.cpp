#include "sequential_server/seq_server.h"
#include "concurrency_server/conc_server.h"
#include <memory>

int main() {
    constexpr int PORT = 9090;
    // std::unique_ptr<seq_server> server = std::make_unique<seq_server>(PORT);
    std::unique_ptr<conc_server> server = std::make_unique<conc_server>(PORT);
    server->Run();
}