#pragma once
#include "iocoro.h"

namespace iocoro
{

struct IP4Address
{
    uint32_t value;

    IP4Address(); // invalid address by default
    IP4Address(uint32_t addr);
    IP4Address(const char* dotn); // accepts dot notation (a.b.c.d)

    // true if not INADDR_NONE
    bool valid() const;
    // returns address in network order
    uint32_t net() const; 

    // INADDR_ANY
    static IP4Address any();
    // INADDR_LOOPBACK
    static IP4Address loopback();
};

class Connection
{
    FileHandle d_handle;
    ContextPollPtr d_poll;
public:
    Connection(int fd = -1);
    Connection(Connection&& c);

    // returns 0 if success, error otherwise
    int connect(const IP4Address& addr, short port);
    int read(char* buf, std::size_t sz);
    int writeAll(const char* buf, std::size_t sz);

    Connection& operator = (Connection&& c)
    {
        d_handle = std::move(c.d_handle);
        d_poll = std::move(c.d_poll);
        return *this;
    }

    // noncopyable
    Connection(const Connection&) = delete;
    Connection& operator = (const Connection&) = delete;

};

class Listener
{
    FileHandle d_handle;
    ContextPollPtr d_poll;
public:
    Listener();

    int bind(IP4Address addr, short port);
    int listen(int backlog);
    bool accept(Connection& conn);
};


}

