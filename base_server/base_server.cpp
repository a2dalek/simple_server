#include "base_server/base_server.h"
#include <iostream>
#include <unistd.h>
#include <cstring>

#define N_BACKLOG 64

int base_server::ServeConnection(int new_sock_fd) {
    ProcessingState state = ProcessingState::WAIT_FOR_MSG;

    if (send(new_sock_fd, "Hello", 5, 0) < 1) {
        std::cout << "Cannot send" << std::endl;
        close(new_sock_fd);
    }

    while (true) {
        char buf[1024];

        int len = recv(new_sock_fd, buf, sizeof(buf), 0);
        if (len < 0) {
            std::cout << "recv" << std::endl;
            close(new_sock_fd);
            return -2;
        } else if (len == 0) {
            break;
        }

        for (int i = 0; i < len; ++i) {
            switch (state) {
            case ProcessingState::WAIT_FOR_MSG:
                if (buf[i] == '^') {
                    state = ProcessingState::IN_MSG;
                }
                break;

            case ProcessingState::IN_MSG:
                if (buf[i] == '$') {
                    state = ProcessingState::WAIT_FOR_MSG;
                } else {
                    buf[i] += 1;
                    if (send(new_sock_fd, &buf[i], 1, 0) < 1) {
                        std::cout << "send error" << std::endl;
                        close(new_sock_fd);
                        return -1;
                    }
                }
                break;
            }
        }
    }

    close(new_sock_fd);
    return 0;
}

int base_server::ListenInetSocket(int portnum) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cout << "ERROR opening socket" << std::endl;
    return -1;
  }

  // This helps avoid spurious EADDRINUSE when the previous instance of this
  // server died.
  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cout << "setsockopt" <<std::endl;
    return -2;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portnum);

  if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cout << "ERROR on binding" << std::endl;
    return -3;
  }

  if (listen(sockfd, N_BACKLOG) < 0) {
    std::cout << "ERROR on listen";
    return -4;
  }

  return sockfd;
}

int base_server::Run() {};