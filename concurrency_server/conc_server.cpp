#include "concurrency_server/conc_server.h"
#include <thread>
#include <sys/socket.h>
#include <iostream>
#include <functional>
#include <error.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

ConcServer::ConcServer(int port, int num_threads) : BaseServer(port) {
    thread_pool = std::make_unique<ThreadPool>(num_threads);
}

int ConcServer::ConnectPeer(int sockfd) {
  
  if (sockfd >= MAXFDS) {
    std::cout << "File description of socket is out of bound";
    return -1;
  }

  // Initialize state to send back a '*' to the peer immediately.
  PeerState* peerstate = &global_state[sockfd];
  peerstate->state = ProcessingState::INITIAL_ACK;
  peerstate->sendbuf[0] = 'H';
  peerstate->sendptr = 0;
  peerstate->sendbuf_end = 1;
  peerstate->status = FdStatus::SEND;

  // Signal that this socket is ready for writing now.
  return 0;
}

int ConcServer::PeerRecv(int sockfd) {
  if (sockfd >= MAXFDS) {
    std::cout << "File description of socket is out of bound" << std::endl;
    return -1;
  }

  PeerState* peerstate = &global_state[sockfd];

  if (peerstate->state == ProcessingState::INITIAL_ACK || peerstate->sendptr < peerstate->sendbuf_end) {
    // Until the initial ACK has been sent to the peer, there's nothing we
    // want to receive. Also, wait until all data staged for sending is sent to
    // receive more data.
    return 0;
  }

  uint8_t buf[1024];
  int nbytes = recv(sockfd, buf, sizeof buf, 0);
  if (nbytes == 0) { // The peer disconnected.
    peerstate->status = FdStatus::DISCONNECTED;
    return 0;
  } else if (nbytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) { // The socket is not *really* ready for recv; wait until it is.
      return 0;
    } else {
      std::cout << "Error when recieving" << std::endl;
      return -1;
    }
  }

  for (int i = 0; i < nbytes; ++i) {
    switch (peerstate->state) {
      case ProcessingState::WAIT_FOR_MSG:
        if (buf[i] == '^') {
          peerstate->state = ProcessingState::IN_MSG;
        }
        break;
      case ProcessingState::IN_MSG:
        if (buf[i] == '$') {
          peerstate->state = ProcessingState::WAIT_FOR_MSG;
        } else {
          peerstate->sendbuf[peerstate->sendbuf_end++] = buf[i] + 1;
          peerstate->status = FdStatus::SEND;
        }
        break;
    }
  }

  // Report reading readiness iff there's nothing to send to the peer as a
  // result of the latest recv.
  return 0;
}

int ConcServer::PeerSend(int sockfd) {
  PeerState* peerstate = &global_state[sockfd];

  if (peerstate->sendptr >= peerstate->sendbuf_end) { // Nothing to send.
    peerstate->status = FdStatus::RECV;
    return 0;
  }

  int sendlen = peerstate->sendbuf_end - peerstate->sendptr;
  int nsent = send(sockfd, &peerstate->sendbuf[peerstate->sendptr], sendlen, 0);
  if (nsent == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    } else {
      std::cout << "Error when sending" << std::endl;
      return -1;
    }
  }

  if (nsent < sendlen) {
    peerstate->sendptr += nsent;
    return 0;
  } else {
    // Everything was sent successfully; reset the send queue.
    peerstate->sendptr = 0;
    peerstate->sendbuf_end = 0;

    // Special-case state transition in if we were in INITIAL_ACK until now.
    if (peerstate->state == ProcessingState::INITIAL_ACK) {
      peerstate->state = ProcessingState::WAIT_FOR_MSG;
    }

    peerstate->status = FdStatus::RECV;

    return 0;
  }
}

int ConcServer::Run() {
  setvbuf(stdout, NULL, _IONBF, 0);
  std::cout << "Serving on port " << port << std::endl;

  int socket_fd = ListenInetSocket(port);
  if (MakeSocketNonBlocking(socket_fd) < 0) {
      std::cout << "Error making socket non blocking" << std::endl;
      return -1;
  }

  // Create epoll instance
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
      std::cerr << "Error creating epoll instance" << std::endl;
      close(socket_fd);
      return -1;
  }

  struct epoll_event sv_socket_event;
  sv_socket_event.events = EPOLLIN;
  sv_socket_event.data.fd = socket_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &sv_socket_event) < 0) {
      std::cout << "Error adding server socket to epoll" << std::endl;
      close(socket_fd);
      close(epoll_fd);
      return -1;
  }

  while (true)
  {   
      int num_events = epoll_wait(epoll_fd, events, MAXFDS, - 1);
      if (num_events < 0) {
          std::cout << "Error in epoll_wait" << std::endl;
          close(socket_fd);
          close(epoll_fd);
          return -1;
      }

      for (unsigned i = 0; i < num_events; ++i) {
        
          if (events[i].data.fd == socket_fd) {
            auto err_code = thread_pool->submit_job([this, socket_fd, epoll_fd] {
              struct sockaddr_in peer_addr;
              socklen_t len_peer_addr = sizeof(peer_addr);

              int new_fd = accept(socket_fd, (struct sockaddr*)&peer_addr, &len_peer_addr);
              if (new_fd < 0) {
                  if (errno == EAGAIN || errno == EWOULDBLOCK) {
                      // std::cout << "Function `accept` returned EAGAIN or EWOULDBLOCK" << std::endl;
                  } else {
                      close(socket_fd);
                      close(epoll_fd);
                      return -1;
                  }
              } else {
                  MakeSocketNonBlocking(new_fd);
                  ConnectPeer(new_fd);

                  struct epoll_event new_event;
                  new_event.events |= EPOLLOUT;
                  new_event.data.fd = new_fd;
                  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &new_event) < 0) {
                      std::cout << "Error adding server socket to epoll" << std::endl;
                      return -1;
                  }
              }

              return 0;
            }); 

            if (err_code.get() < 0) return err_code.get();
          } else {
              if (events[i].events & EPOLLIN) {
                int fd = events[i].data.fd;

                auto err_code = thread_pool->submit_job([this, fd, epoll_fd] {
                  PeerRecv(fd);

                  struct epoll_event new_event;
                  FdStatus status = global_state[fd].status;
                  if (status == FdStatus::RECV) {
                    new_event.events |= EPOLLIN;
                  } else if (status == FdStatus::SEND) {
                    new_event.events |= EPOLLOUT;
                  }
                  new_event.data.fd = fd;

                  if (new_event.events == 0) {
                      std::cout << "Closing socket " << fd << std::endl;

                      if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                        std::cout << "Error when deleting new_event" << std::endl;
                        close(fd);
                        return -1;
                      }

                      close(fd);
                  } else if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &new_event) < 0) {
                      std::cout << "Error when modifying new_event" << std::endl;
                      close(fd);
                      return -1;
                  }

                  return 0;
                });

                if (err_code.get() < 0) return err_code.get();
              } else if (events[i].events & EPOLLOUT) {
                int fd = events[i].data.fd;

                auto err_code = thread_pool->submit_job([this, fd, epoll_fd] {
                  PeerSend(fd);

                  struct epoll_event new_event;
                  FdStatus status = global_state[fd].status;
                  if (status == FdStatus::RECV) {
                    new_event.events |= EPOLLIN;
                  } else if (status == FdStatus::SEND) {
                    new_event.events |= EPOLLOUT;
                  }
                  new_event.data.fd = fd;

                  if (new_event.events == 0) {
                      std::cout << "Closing socket " << fd << std::endl;

                      if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                        std::cout << "Error when deleting new_event" << std::endl;
                        close(fd);
                        return -1;
                      }

                      close(fd);
                  } else if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &new_event) < 0) {
                      std::cout << "Error when modifying new_event";
                      close(fd);
                      return -1;
                  }
                });

                if (err_code.get() < 0) return err_code.get();
              }
          }
      }
  } 

  return 0;
}

int ConcServer::MakeSocketNonBlocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        std::cout << "Error getting socket flags" << std::endl;
        close(fd);
        return -1;
    }

    if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1) {
        std::cout << "Error getting socket flags" << std::endl;
        close(fd);
        return -2;
    }

    return 0;
}