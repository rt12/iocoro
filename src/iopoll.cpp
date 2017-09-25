#include "iopoll.h"
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>

namespace iocoro
{

Poller::Poller() 
    : d_efd(epoll_create1(0)), d_count(0) {}

void Poller::add(FilePoll* pd)
{
    fprintf(stderr, "Poller: add fd %d\n", pd->fd);
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET; 
    ev.data.ptr = pd;
    int r = epoll_ctl(d_efd.handle(), EPOLL_CTL_ADD, pd->fd, &ev);
    if (r < 0) {
        fprintf(stderr, "Poller: add fd %d failed: %d\n", pd->fd, errno);
        return;
    }

    ++d_count;
}

void Poller::remove(FilePoll* pd)
{
    fprintf(stderr, "Poller: remove fd %d\n", pd->fd);
    epoll_event ev;
    ev.events = 0; 
    ev.data.ptr = pd;
    int r = epoll_ctl(d_efd, EPOLL_CTL_DEL, pd->fd, &ev);
    if (r < 0) {
        fprintf(stderr, "Poller: remove fd %d failed: %d\n", pd->fd, errno);
        return;
    }

    --d_count;
}

int Poller::wait(int timeoutMs)
{
    const size_t MAXEVENTS = 1024;
    epoll_event events[MAXEVENTS]; 

    int n = epoll_wait(d_efd.handle(), events, MAXEVENTS, timeoutMs);

    if (n == -1) {
        return n;
    }

    for (int i = 0; i < n; ++i) {
        auto& ev = events[i];

        uint32_t flags = 0;

        if (ev.events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            flags |= EventType::Read; 
        }

        if (ev.events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
            flags |= EventType::Write;
        }

        static_cast<FilePoll*>(ev.data.ptr)->handleEvents(flags);
    }

    return n;
}

}

