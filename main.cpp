#include "sequential_server/seq_server.h"
#include <memory>

int main() {
    constexpr int PORT = 9090;
    std::unique_ptr<seq_server> server = std::make_unique<seq_server>(PORT);
    server->Run();
}