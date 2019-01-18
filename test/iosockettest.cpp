#include <gtest/gtest.h>
#include <iosocket.h>
#include <thread>

namespace iocoro
{

class SocketTest: public testing::Test
{
public:
    Dispatcher d;
    Listener listener;
    
    void client()
    {
        Connection c;
        IP4Endpoint addr(IP4Address::loopback(), 8099);
        int r = c.connect(addr);
        ASSERT_EQ(0, r);
        ASSERT_EQ(addr, c.remoteAddress());
        // add a little pause to make sure other coroutine will have to wait for io
        Context::sleep_for(std::chrono::milliseconds(100));
        c.writeAll("hello", 5);
 
        char buf[10];
        ASSERT_EQ(5, c.read(buf, sizeof(buf)));

        ASSERT_EQ(0, memcmp("world", buf, 5));
    }

    void server(int connections)
    {
        ASSERT_EQ(0, listener.bind(IP4Endpoint(IP4Address::any(), 8099)));
        ASSERT_EQ(0, listener.listen(2048));

        acceptConnection(connections);
    }

    void acceptConnection(int connections) {
        Connection conn;
        ASSERT_TRUE(listener.accept(conn));

        if (connections > 1) {
            d.spawn([this, connections]{ 
                acceptConnection(connections-1); 
            });
        }

        char buf[100];
        ASSERT_EQ(5, conn.read(buf, 100));
        ASSERT_EQ(0, memcmp("hello", buf, 5));

        ASSERT_EQ(0, conn.writeAll("world", 5));
    }
};


TEST_F(SocketTest, connect)
{
    d.spawn([this] { server(0); });
    d.spawn([this] { client(); });
    d.dispatch();
}

TEST_F(SocketTest, manyConnections)
{
    d.spawn([this] { server(10); });

    for (int i = 0; i < 10; ++i) {
        d.spawn([this] { client(); });
    }
}

}

