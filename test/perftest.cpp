#include <iocoro.h>
#include <gtest/gtest.h>
#include <iostream>

using namespace iocoro;

const std::size_t ITER = 100000;

class Perf: public testing::Test
{
protected:
    Clock::time_point start;
    std::size_t total = 0;

public:
    virtual void SetUp() override
    {
        start = Clock::now();
    }

    virtual void TearDown() override
    {
        auto dur = Clock::now() - start;
        std::cout << "Total time: " << std::chrono::nanoseconds(dur).count() << "ns" << std::endl;
        std::cout << "Switch time: " << std::chrono::nanoseconds(dur).count()/total<< "ns" << std::endl;
    }
};

TEST_F(Perf, Callcc)
{
    auto start = Clock::now();
    auto c = boost::context::callcc([&](boost::context::continuation&& c) {
        for (int i = 0; i < ITER; ++i) {
            c = c.resume();
        } 
        return c.resume();
    });

    for (int i = 0; i < ITER; ++i) {
        c = c.resume();
    }
    
    total = ITER*2;
}

TEST_F(Perf, Switch)
{
    Dispatcher d;

    auto f = [&, this]() {
        for (int i = 0; i < ITER; ++i) {
            ++total;
            Context::yield();
        }
    };

    d.spawn(f);
    d.dispatch();
}

TEST_F(Perf, SwitchWithSleepers)
{
    Dispatcher d;
    Event done;

    auto sleeper = [&] {
        done.wait();
    };

    auto f = [&, this]() {
        // add a bunch of sleeping coroutines
        for (int i = 0; i < 10000; ++i) {
            d.spawn(sleeper);
        }

        for (int i = 0; i < ITER; ++i) {
            ++total;
            Context::yield();
        }

        done.notify_all();
    };

    d.spawn(f);
    d.dispatch();
}

TEST_F(Perf, Spawn)
{
    Dispatcher d;

    auto inner = [this]{
        ++total;
    };

    auto f = [&, this]() {
        for (int i = 0; i < ITER; ++i) {
            d.spawn(inner);
        }
    };

    for (int i = 0; i < 4; ++i) {
        d.spawn(f);
    }

    d.dispatch();
}

