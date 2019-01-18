#include "iosocket.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


namespace iocoro {

namespace {

sockaddr_in toSockAddr(const IP4Endpoint& endpoint)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.port);
    addr.sin_addr.s_addr = htonl(endpoint.addr.value);
    return addr;
}

IP4Endpoint toIP4Endpoint(const sockaddr_in& addr)
{
    return IP4Endpoint(
            IP4Address(ntohl(addr.sin_addr.s_addr)),
            ntohs(addr.sin_port));
}

int make_socket_non_blocking (int sfd) 
{
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}

} // end anonymous namespace

IP4Address::IP4Address() : value(INADDR_NONE) {}
IP4Address::IP4Address(uint32_t addr) : value(addr) {}
IP4Address::IP4Address(const char* dotn)
{
    value = inet_addr(dotn);
}

IP4Address IP4Address::any()
{
    return IP4Address(INADDR_ANY);
}

IP4Address IP4Address::loopback()
{
    return IP4Address(INADDR_LOOPBACK);
}

//////////////////////////////////////////////////////////////////////////
// class Connection 
//////////////////////////////////////////////////////////////////////////
Connection::Connection(int fd) 
{
    attach(fd);
}

void Connection::attach(int fd)
{
    d_handle = FileHandle(fd);
    d_poll.add(fd);
}

int Connection::connect(const IP4Endpoint& endpoint)
{
    FileHandle fd;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        throw std::runtime_error("Failed to create socket");
    make_socket_non_blocking(sfd);

    fd = sfd;
    ContextPoll poll(sfd);

    sockaddr_in serv_addr = toSockAddr(endpoint);

    int r = ::connect(sfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

    if (r < 0) {
        if (errno != EINPROGRESS)
            return errno;
        poll.waitWrite();
    } 

    int result;
    socklen_t result_len = sizeof(result);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
        fprintf(stderr, "getsockopt() failed\n");
        // error, fail somehow, close socket
        return errno;
    };

    if (result != 0) {
        return result;
    }

    d_remoteAddr = endpoint;
    d_handle = std::move(fd);
    d_poll = std::move(poll);
    return result;
}

int Connection::read(char* buf, std::size_t sz)
{
    for (;;) {
        int r = ::recv(d_handle, buf, sz, 0);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                d_poll.waitRead();
            } else {
                return r;
            }
        } else {
            return r;
        }
    }
}

int Connection::writeAll(const char* buf, std::size_t sz)
{
    std::size_t szLeft = sz; 
    while(szLeft != 0) {
        int r = ::write(d_handle, buf, szLeft);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                d_poll.waitWrite();
            } else {
                return r;
            }
        } else {
            buf += r;
            szLeft -= r;
        }
    }

    return 0;
}

void Connection::shutdown()
{
    ::shutdown(d_handle, SHUT_RDWR);
}

//////////////////////////////////////////////////////////////////////////
// class Listener
//////////////////////////////////////////////////////////////////////////
Listener::Listener()
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        throw std::runtime_error("Failed to create socket");

    int enable = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int));

    make_socket_non_blocking(sfd);

    d_handle = FileHandle(sfd);
}

int Listener::bind(const IP4Endpoint& endpoint)
{
    auto serv_addr = toSockAddr(endpoint);
    int retval = ::bind(d_handle, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (retval != 0) {
        fprintf(stderr, "Could not bind\n");
        return errno;
    }

    return retval;
}

int Listener::listen(int backlog)
{
    int r = ::listen(d_handle.handle(), backlog);

    if (r != 0) {
        return errno;
    }

    d_poll.add(d_handle);
    return r;
}

bool Listener::accept(Connection& conn)
{
    sockaddr_in inAddr;
    socklen_t inLen = sizeof(inAddr);

    for (;;) {
        int infd = ::accept(d_handle.handle(), (sockaddr*)&inAddr, &inLen);
        if (infd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "accept4 failed: %d\n", errno);
                return false;
            }
            d_poll.waitRead();
        } else {
            make_socket_non_blocking(infd);

            int enable = 1;
            setsockopt(infd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
            conn.attach(infd);
            conn.setRemoteAddress(toIP4Endpoint(inAddr));
            return true;
        }
    }
}

}

