#include "sequential_server/seq_server.h"
#include "concurrency_server/conc_server.h"
#include <memory>

int main() {
    constexpr int PORT = 9090;
    // std::unique_ptr<SeqServer> server = std::make_unique<SeqServer>(PORT);
    std::unique_ptr<ConcServer> server = std::make_unique<ConcServer>(PORT, 3);
    server->Run();
}