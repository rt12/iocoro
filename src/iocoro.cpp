#include "iocoro.h"
#include <assert.h>

#include <thread>
#include <iostream>

namespace ctx = boost::context;

namespace iocoro 
{

thread_local Context* tls_currentContext = nullptr;
//////////////////////////////////////////////////////////////////////////
// class ContextPoll
//////////////////////////////////////////////////////////////////////////

ContextPoll::ContextPoll(int f)
: FilePoll(f)
{
    add(f);
}

ContextPoll::ContextPoll(ContextPoll&& ctx)
: FilePoll(ctx.fd)
{
    *this = std::move(ctx);
}

ContextPoll::~ContextPoll()
{
    remove();
}

void ContextPoll::add(int f)
{
    remove();
    fd = f;

    if (fd != -1)
        getCurrentPoller().add(this);
}

void ContextPoll::remove()
{
    if (fd != -1) {
        getCurrentPoller().remove(this);
        fd = -1;
    }
}

void ContextPoll::handleEvents(uint32_t events)
{
    if (events & EventType::Write) {
        if (writeContext != nullptr) {
            writeContext->enable();
        }
    }

    if (events & EventType::Read) {
        if (readContext != nullptr) {
            readContext->enable();
        }
    }
}

void ContextPoll::waitRead()
{
    if (readContext != nullptr)
        throw std::runtime_error("Another context is reading");

    readContext = Context::self();
    readContext->disable();
    Context::yield();
    readContext = nullptr;
}

void ContextPoll::waitWrite()
{
    if (writeContext != nullptr)
        throw std::runtime_error("Another context is writing");

    writeContext = Context::self();
    writeContext->disable();
    Context::yield();
    writeContext = nullptr;
}

ContextPoll& ContextPoll::operator = (ContextPoll&& ctx)
{
    int newFd = ctx.fd;
    ctx.remove();
    add(newFd);

    return *this;
}

//////////////////////////////////////////////////////////////////////////
// class Context
//////////////////////////////////////////////////////////////////////////
Context::~Context()
{
}

void Context::cc()
{
    d_coro = d_coro.resume();
}

void Context::init(std::function<void()>&& entry) 
{ 
    d_entry = std::move(entry); 
    d_finished = false;
    d_deadline = Clock::time_point::min();
}

bool Context::resume(uint32_t wakeFlags)
{
    if (d_finished) {
        return false;
    }

    d_wakeFlags = wakeFlags;
    tls_currentContext = this;

    if (d_coro) {
        cc();
    } else {
        d_coro = ctx::callcc([this](ctx::continuation&& c) {
            d_coro = std::move(c);

            // allow context to be reused (set different entry function)
            for (;;) {
                d_entry();
                d_finished = true;
                d_coro = d_coro.resume();
            }

            return d_coro.resume();
        });
    }    

    tls_currentContext = nullptr;
    return !d_finished;
}

void Context::disable()
{
    // context will not be scheduled until explicitely resumed
    schedule(Clock::time_point::max());
}

void Context::enable()
{
    // set context to run as soon as possible
    schedule(Clock::time_point::min());
}

void Context::schedule(Clock::time_point deadline)
{
    d_dispatcher.schedule(this, deadline);
}

Context* Context::self()
{
    return tls_currentContext;
}

uint32_t Context::yield()
{
    self()->cc();
    return self()->d_wakeFlags;
}

void Context::sleep_for(Clock::duration d)
{
    sleep_until(Clock::now() + d);
}

void Context::sleep_until(Clock::time_point t)
{
    self()->schedule(t);
    self()->cc();
}

//////////////////////////////////////////////////////////////////////////
// class Event
//////////////////////////////////////////////////////////////////////////
void Event::notify(bool one)
{
    d_signalled = true;
    while (d_begin != nullptr) {
        auto ctx = d_begin->ctx;
        d_begin = d_begin->next;
        ctx->enable();
        if (one) {
            break;
        }
    }

    // let scheduler call enabled contexts
    Context::yield();
    d_signalled = false;
}

void Event::addWaiter(WaiterNode& node)
{
    if (d_begin == nullptr) {
        d_begin = &node;
        d_end = &node;
    } else {
        d_end->next = &node;
        d_end = &node;
    }
}

void Event::notify_one()
{
    notify(true);
}

void Event::notify_all()
{
    notify(false);
}

void Event::wait()
{
    if (d_signalled)
        return;

    auto ctx = Context::self();
    WaiterNode node{ctx, 0};
    addWaiter(node);
    // Start waiting
    ctx->disable();
    Context::yield();
}

bool Event::wait_for(Clock::duration d)
{
    if (d_signalled)
        return true;

    auto ctx = Context::self();
    WaiterNode node{ctx, 0};
    addWaiter(node);
    // Start waiting
    ctx->schedule(Clock::now() + d);
    Context::yield();

    return d_signalled;
}

//////////////////////////////////////////////////////////////////////////
// class Dispatcher 
//////////////////////////////////////////////////////////////////////////
Dispatcher::Dispatcher()
{
}

Dispatcher::~Dispatcher()
{
    fprintf(stderr, "Dispatcher: %zu contexts in cache\n", d_unused.size());
}

void Dispatcher::schedule(Context* ctx, const Clock::time_point& deadline)
{
    auto& srcList = getListByDeadline(ctx->d_deadline);
    auto& dstList = getListByDeadline(deadline);

    // update Context's value
    ctx->d_deadline = deadline; 

    // update closest deadline if needed 
    if (deadline < d_deadline) {
        d_deadline = deadline;
    }

    if (&srcList == &dstList)
        return;

    // move to other list
    dstList.splice(dstList.end(), srcList, srcList.iterator_to(*ctx));
}

boost::intrusive::list<Context>& 
Dispatcher::getListByDeadline(const Clock::time_point& deadline)
{
    if (deadline == Clock::time_point::min())
        return d_ready;

    if (deadline == Clock::time_point::max())
        return d_disabled;

    return d_sleeping;
}

void Dispatcher::spawn(std::function<void()>&& f)
{
    Context* ctx = nullptr;

    if (d_unused.empty()) {
        ctx = d_pool.construct(*this);
    } else {
        ctx = &d_unused.front();
        d_unused.pop_front();
    } 

    ctx->init(std::move(f));
    d_ready.push_back(*ctx);

    if (Context::self()) {
        // if we are running in a coroutine,
        // give the new one a chance to start
        Context::yield();
    }
}

static int msToDeadline(const Clock::time_point& deadline)
{
    if (deadline == Clock::time_point::min())
        return 0;

    if (deadline == Clock::time_point::max())
        return -1;

    const int64_t NS_IN_MS = 1000000;
    // use nano precision to get correct rounding up
    auto dur = DurationNano(deadline - Clock::now()).count(); 
    if (dur <= 0) {
        return 0;
    }

    return (dur + NS_IN_MS - 1) / NS_IN_MS;
}

void Dispatcher::dispatch()
{
    for (;;) {
        d_now = Clock::now();

        if (d_now >= d_deadline) {
            // we reached at least one deadline
            // move contexts to ready and update closest deadline time
            d_deadline = TimePoint::max();

            for (auto it = d_sleeping.begin(); it != d_sleeping.end();) {
                // deadline passed, move to ready 
                auto nextIt = std::next(it);
                if (d_now >= it->d_deadline) {
                    // advance to next element and move context to ready list
                    auto& ctx = *it;
                    schedule(&ctx, TimePoint::min());
                } else if (it->d_deadline < d_deadline) {
                    d_deadline = it->d_deadline;
                }
                it = nextIt;
            }
        }

        for (auto it = d_ready.begin(); it != d_ready.end();) {
            assert(it->d_deadline == TimePoint::min());

            auto nextIt = std::next(it);
            if (!it->resume()) {
                // move finished context to the list to be reused
                d_unused.splice(d_unused.begin(), d_ready, it);
            }

            it = nextIt;
        }

        // if all lists are empty, then there is no more work
        if (d_ready.empty() && d_sleeping.empty() && d_disabled.empty())
            break;

        int pollerTimeout = d_ready.empty() ? msToDeadline(d_deadline) : 0;
        int n = d_poller.wait(pollerTimeout);
    }
}

void Dispatcher::stop()
{
    d_stop = true;
}

}

