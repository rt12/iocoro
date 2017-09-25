#include "iosocket.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


namespace {


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

int create_and_bind(short port) 
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        return sfd;

    int enable = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int));

    setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    setsockopt(sfd, IPPROTO_TCP, TCP_QUICKACK, &enable, sizeof(int));

    int retval = bind(sfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (retval != 0) {
        close(sfd);
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    return sfd;
}
}

namespace iocoro
{

namespace
{

ContextPollPtr getContextPoll(int fd)
{
    return Context::self()->dispatcher().getPoll(fd);
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
Connection::Connection(int fd) : d_handle(fd) 
{
    if (fd != -1) {
        d_poll = Context::self()->dispatcher().getPoll(fd);
    }
}

Connection::Connection(Connection&& c) 
{
    *this = std::move(c);
}

int Connection::connect(const IP4Address& addr, short port)
{
    FileHandle fd;
    ContextPollPtr poll;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        throw std::runtime_error("Failed to create socket");
    make_socket_non_blocking(sfd);

    fd = sfd;
    poll = getContextPoll(sfd);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(addr.value);

    int r = ::connect(sfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

    if (r < 0) {
        if (errno != EINPROGRESS)
            return errno;
        poll->waitWrite();
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
                d_poll->waitRead();
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
    for (;;) {
        int r = ::write(d_handle, buf, szLeft);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                d_poll->waitWrite();
            } else {
                return r;
            }
        } else {
            buf += r;
            szLeft -= r;
            if (szLeft == 0)
                return r;
        }
    }
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

    setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    setsockopt(sfd, IPPROTO_TCP, TCP_QUICKACK, &enable, sizeof(int));

    make_socket_non_blocking(sfd);

    d_handle = FileHandle(sfd);
}

int Listener::bind(IP4Address addr, short port)
{
    int sfd = d_handle.handle();
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(addr.value);

    int retval = ::bind(sfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (retval != 0) {
        close(sfd);
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    return retval;
}

int Listener::listen(int backlog)
{
    int r = ::listen(d_handle.handle(), backlog);

    if (r < 0) {
        return r;
    }

    d_poll = Context::self()->dispatcher().getPoll(d_handle);
    return r;
}

bool Listener::accept(Connection& conn)
{
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);

    for (;;) {
        int infd = accept4(d_handle.handle(), &in_addr, &in_len, SOCK_NONBLOCK);
        if (infd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "accept4 failed: %d\n", errno);
                return false;
            }
            d_poll->waitRead();
        } else {
            conn = Connection(infd);
            return true;
        }
    }

    return false;
}

}

