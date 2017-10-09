#pragma once

#include "iocommon.h"
#include "iopoll.h"

#include <chrono>
#include <functional>
#include <list>
#include <memory>

#include <boost/context/continuation.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/intrusive/list.hpp>

namespace iocoro
{

typedef std::chrono::steady_clock Clock;
typedef Clock::time_point TimePoint;
typedef std::chrono::duration<double, std::milli> DurationMilli;
typedef std::chrono::duration<int64_t, std::nano> DurationNano;

class Dispatcher;
class Context;

namespace Flags
{
    const uint32_t None = 0;
    // Wake flags
    const uint32_t Interrupt  = 0x100; // break execution
    const uint32_t Schedule   = 0x200; // scheduled wake
};

class ContextPoll: FilePoll
{
    Context* readContext{nullptr};
    Context* writeContext{nullptr};

    virtual void handleEvents(uint32_t events) override;

public:
    ContextPoll(int fd = -1);
    ~ContextPoll();

    void add(int fd);
    void remove();
    void waitRead();
    void waitWrite();

    // move
    ContextPoll(ContextPoll&& ctx);
    ContextPoll& operator = (ContextPoll&& ctx);

    // noncopyable
    ContextPoll(const ContextPoll&) = delete;
    ContextPoll& operator = (const ContextPoll&) = delete;
};

class Context: 
    public boost::intrusive::list_base_hook<>
{
    friend class Dispatcher;

    Dispatcher& d_dispatcher;
    std::function<void()> d_entry;
    boost::context::continuation d_coro;
    bool d_finished = false;
    Clock::time_point d_deadline{Clock::time_point::min()};
    uint32_t d_wakeFlags{Flags::None};

    void cc();
    bool resume(uint32_t flags = Flags::None);
    void init(std::function<void()>&& entry);

public:
    Context(Dispatcher& dispatcher) 
        : d_dispatcher(dispatcher) {}
    ~Context();

    void disable();
    void enable();
    void schedule(Clock::time_point deadline);
    Dispatcher& dispatcher() { return d_dispatcher; }

    static Context* self();
    static uint32_t yield();
    static void sleep_for(Clock::duration d);
    static void sleep_until(Clock::time_point t);
};

class Event
{
    struct WaiterNode {
        Context* ctx;
        WaiterNode* next;
    };

    WaiterNode* d_begin{nullptr};
    WaiterNode* d_end{nullptr};
    bool d_signalled{false};

    void notify(bool one);
    void addWaiter(WaiterNode& node);

public:
    void notify_one();
    void notify_all();
    void wait();
    bool wait_for(Clock::duration d);
};

class Dispatcher
{
    boost::object_pool<Context> d_pool;

    // Internal time
    TimePoint d_now{TimePoint::min()};
    TimePoint d_deadline{TimePoint::max()};
    
    // scheduling lists
    boost::intrusive::list<Context> d_ready;
    boost::intrusive::list<Context> d_sleeping;
    boost::intrusive::list<Context> d_disabled;
    boost::intrusive::list<Context> d_unused;

    Poller d_poller;
    bool d_stop;

    boost::intrusive::list<Context>& 
        getListByDeadline(const Clock::time_point& deadline);

public:
    Dispatcher();
    ~Dispatcher();

    void spawn(std::function<void()>&& f);
    void dispatch();
    void stop();

    // internal
    void schedule(Context* ctx, const Clock::time_point& deadline);
    ContextPoll getPoll(int fd);
    Poller& getPoller() { return d_poller; }
};

inline Poller& getCurrentPoller()
{
    return Context::self()->dispatcher().getPoller();
}


}

