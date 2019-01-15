#include "iopoll.h"

#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>

namespace {

const int MAX_KQUEUE_EVENTS = 64;

struct timespec *toTimespec(struct timespec *t, int ms)
{
    if (ms < 0)
        return NULL;

    t->tv_sec = ms / 1000;
    t->tv_nsec = (ms % 1000) * 1000000;

    return t;
}

}

namespace iocoro
{

Poller::Poller() : d_efd(kqueue()) {
}

void Poller::add(FilePoll* pd)
{
    struct kevent ev[2];

    EV_SET(&ev[0], pd->fd, EVFILT_READ, EV_ADD, 0, 0, pd);
    EV_SET(&ev[1], pd->fd, EVFILT_WRITE, EV_ADD, 0, 0, pd);
    
    // TODO: handle error
    kevent(d_efd, ev, 2, 0, 0, 0);

    ++d_count;
}

void Poller::remove(FilePoll* pd)
{
    struct kevent ev;
    EV_SET(&ev, pd->fd, 0, EV_DELETE, 0, 0, pd);

    // TODO: handle error
    kevent(d_efd, &ev, 1, 0, 0, 0);

    --d_count;
}

int Poller::wait(int timeoutMs)
{
    struct kevent ev[MAX_KQUEUE_EVENTS];
    struct timespec timeout;

    int n = kevent(d_efd, 0, 0, ev, MAX_KQUEUE_EVENTS, toTimespec(&timeout, timeoutMs));
    if (n < 0)
        return n;

    for (int i = 0; i < n; ++i) {
        auto& kev = ev[i];

        uint32_t flags{0};

        if (kev.filter == EVFILT_READ) {
            flags |= EventType::Read;
        } else if (kev.filter == EVFILT_WRITE) {
            flags |= EventType::Write;
        }

        static_cast<FilePoll*>(kev.udata)->handleEvents(flags);
    }

    return 0;
}

}
