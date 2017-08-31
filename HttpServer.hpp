#ifndef HLCUP_HTTPSERVER_HPP__
#define HLCUP_HTTPSERVER_HPP__

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>

#include "Connection.hpp"
#include "Storage.hpp"

namespace hlcup {

/* -------------------------------------------------------------------
 * If bufsize > 0, set the TCP window size (via the socket buffer
 * sizes) for sock. Otherwise leave it as the system default.
 *
 * This must be called prior to calling listen() or connect() on
 * the socket, for TCP window sizes > 64 KB to be effective.
 *
 * This now works on UNICOS also, by setting TCP_WINSHIFT.
 * This now works on AIX, by enabling RFC1323.
 * returns -1 on error, 0 on no error.
 * -------------------------------------------------------------------
 */

int set_tcp_windowsize(int sock, int bufsize, int dir) {
    int rc;
    int newbufsize;

    if (bufsize > 0) {
        /*
         * note: results are verified after connect() or listen(), since
         * some OS's don't show the corrected value until then.
         */
        //        printf("Setting TCP buffer to size: %d\n", bufsize);
        newbufsize = bufsize;
        rc = setsockopt(sock, SOL_SOCKET, dir, (char *)&newbufsize, sizeof(newbufsize));
        if (rc < 0) return rc;
    } else {
        //        printf("      Using default TCP buffer size and assuming OS will do autotuning\n");
    }

    return 0;
}

/* -------------------------------------------------------------------
 * returns the TCP window size (on the sending buffer, SO_SNDBUF),
 * or -1 on error.
 * ------------------------------------------------------------------- */

int get_tcp_windowsize(int sock, int dir) {
    int bufsize = 0;

    int rc;
    socklen_t len;

    /* send buffer -- query for buffer size */
    len = sizeof bufsize;
    rc = getsockopt(sock, SOL_SOCKET, dir, (char *)&bufsize, &len);

    if (rc < 0) return rc;

    return bufsize;
}

class HttpServer {
public:
    static const size_t kMaxEvents = 8192;
    static const int kReadBufSize = 8192;

    HttpServer() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            perror("epoll_create1()");
            abort();
        }

        server_sock_ = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        if (server_sock_ == -1) {
            perror("socket()");
            abort();
        }
        int value = 1;
        setsockopt(server_sock_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    }

    void startListener(int port) {
        port_ = port;

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        int err;
        CALL_RETRY(err, bind(server_sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));
        if (err == -1) {
            perror("bind()");
            abort();
        }

        CALL_RETRY(err, listen(server_sock_, 4096));
        if (err == -1) {
            perror("listen()");
            abort();
        }
    }

    void run() {
        epoll_event evt;
        std::array<epoll_event, kMaxEvents> evts;

        evt.data.ptr = nullptr;
        evt.events = EPOLLIN | EPOLLET;

        int err;
        CALL_RETRY(err, epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_sock_, &evt));
        if (err == -1) {
            perror("epoll_ctl()");
            abort();
        }

        for (;;) {
            int evts_cnt = epoll_wait(epoll_fd_, evts.data(), kMaxEvents, -1);
            //            if (evts_cnt < 0) {
            //                perror("epoll_wait()");
            //                abort();
            //            }
            for (int i = 0; i < evts_cnt; ++i) {
                epoll_event &ev = evts[i];
                Connection *conn = static_cast<Connection *>(ev.data.ptr);

                if (ev.events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    if (conn != nullptr) {
                        if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->fd(), &ev)) {
                            perror("epoll_ctl(EPOLL_CTL_DEL)");
                        } else {
                            delete conn;
                        }
                        //                            std::cout << "Close " << conn->fd() << std::endl;

                    } else {
                        std::cout << "Error on server sock" << std::endl;
                    }
                } else if (ev.events & EPOLLIN) {
                    if (conn == nullptr) {
                        for (;;) {
                            int csock;
                            CALL_RETRY(csock, ::accept4(server_sock_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
                            if (csock == -1) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    // We have processed all incoming connections
                                    break;
                                } else {
                                    perror("accept4()");
                                    break;
                                }
                            }

                            set_tcp_windowsize(csock, 1024 * 1024, SO_SNDBUF);
                            Connection *newconn = new Connection(csock);
                            evt.data.ptr = newconn;
                            evt.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

                            CALL_RETRY(err, epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, csock, &evt));
                            if (err == -1) {
                                perror("epoll_ctl()");
                                abort();
                            }
                        }

                    } else {
                        char buf[kReadBufSize];

                        for (;;) {
                            int count = conn->read(buf, kReadBufSize);
                            if (count == -1) {
                                if (errno != EAGAIN) {
                                    perror("read()");
                                }
                                break;
                            } else if (count == 0) {
                                break;
                            } else {
                                if (conn->handleData(buf, count)) {
                                    delete conn;
                                }
                                if (count < kReadBufSize) break;
                            }
                        }
                    }
                }
            }
        }
    }

    ~HttpServer() {
        close(server_sock_);
        close(epoll_fd_);
    }

private:
    int epoll_fd_;
    int server_sock_;
    int port_;
};
}

#endif
