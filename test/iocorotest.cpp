#include <gtest/gtest.h>
#include <iocoro.h>

using namespace iocoro;

TEST(Base, Success)
{
    Dispatcher d;
    int i = 0;
    int n = 0;

    d.spawn([&] {
        i += 2;
        printf("Co1: step1\n");
        Context::yield();
        printf("Co1: step2\n");
        n += 3;
    });

    d.spawn([&] {
        i += 3;
        printf("Co2: step1\n");
        Context::yield();
        printf("Co2: step2\n");
        n += 4;
        Context::yield();
        printf("Co2: step3\n");
    });

    EXPECT_EQ(0, i);
    EXPECT_EQ(0, n);

    d.dispatch();

    EXPECT_EQ(5, i);
    EXPECT_EQ(7, n);
}

TEST(Base, Nested)
{
    Dispatcher d;
    int i = 0;

    d.spawn([&] {
        i = 2;
        d.spawn([&] {
            i += 3;
            Context::yield();
            i += 10;
        });
    });

    d.dispatch();
    EXPECT_EQ(15, i);
}

TEST(Base, Nested2)
{
    Dispatcher d;
    int i = 0;

    d.spawn([&] {
        i = 2;
        d.spawn([&] {
            i += 3;
            Context::yield();
            i += 10;
        });

        while (i != 5)
            Context::yield();
        i = 2;
    });

    d.dispatch();
    EXPECT_EQ(12, i);
}

TEST(Base, NestedMany)
{
    Dispatcher d;
    int spawns = 0;
    const int outerLoop = 10;
    const int innerLoop = 20;

    for (int i = 0; i < outerLoop; ++i) {
        d.spawn([&] {
            for (int j = 0; j < innerLoop; ++j) { 
                d.spawn([&] { ++spawns; });
                if (j % 3 == 0) {
                    // just for fun
                    Context::yield();
                }
            }
        });
    }  

    d.dispatch();

    EXPECT_EQ(outerLoop * innerLoop, spawns);
}

Clock::duration ms30 = std::chrono::milliseconds(30);
Clock::duration ms50 = std::chrono::milliseconds(50);

double elapsed(Clock::time_point start)
{
    return DurationMilli(Clock::now() - start).count();
}

TEST(Timer, oneTimeout)
{
    Dispatcher d;

    d.spawn([&] {
        auto start = Clock::now();
        Context::sleep_for(ms50);
        // allow 5ms skew 
        EXPECT_NEAR(50, elapsed(start), 5);
    });

    d.dispatch();
}


TEST(Timer, severalTimeouts)
{
    Dispatcher d;
    auto start = Clock::now();

    d.spawn([&] {
        Context::sleep_for(ms30);
        EXPECT_NEAR(30, elapsed(start), 5);
        Context::sleep_for(ms50);
        EXPECT_NEAR(80, elapsed(start), 5);
    });

    d.spawn([&] {
        Context::sleep_for(ms50);
        EXPECT_NEAR(50, elapsed(start), 5);
        Context::sleep_for(ms50);
        EXPECT_NEAR(100, elapsed(start), 5);
    });

    d.dispatch();
}

TEST(Event, notifyOne)
{
    Dispatcher d;
    Event e;
    bool received = false;

    d.spawn([&] {
        e.wait();
        received = true;
    });
    
    d.spawn([&] {
        e.notify_one();
    });

    d.dispatch();

    ASSERT_TRUE(received);
}

TEST(Event, notifyAll)
{
    Dispatcher d;
    Event e;

    const int waiters = 10;
    int received = 0;

    for (int i = 0; i < waiters; ++i) {
        d.spawn([&] {
            e.wait();
            ++received;
        });
    } 
    
    d.spawn([&] {
        e.notify_all();
    });

    d.dispatch();

    ASSERT_EQ(waiters, received);
}

TEST(Event, pingPong)
{
    Dispatcher d;
    Event e1;
    Event e2;

    d.spawn([&] {
       e1.wait();
       e2.notify_one();
    });
    
    d.spawn([&] {
       e1.notify_all();
       e2.wait();
    });

    d.dispatch();
}

TEST(Event, invertedOrder)
{
    Dispatcher d;
    Event e;
    Event allStarted;

    const int waiters = 3;
    int received = 0;
    int started = 0; 

    d.spawn([&] {
        printf("Waiting for all to start...\n");
        allStarted.wait();
        printf("All started, sending notification\n");
        e.notify_all();
        printf("Finished notify_all\n");
    });

    for (int i = 0; i < waiters; ++i) {
        d.spawn([&, i] {
            if (++started == waiters) {
                printf("All started, notify_one\n");
                allStarted.notify_one();
            } 
            printf("Wait %d\n", i + 1);
            e.wait();
            printf("Wait %d completed\n", i + 1);
            ++received;
        });
    } 
    
    d.dispatch();

    ASSERT_EQ(waiters, received);
}    


