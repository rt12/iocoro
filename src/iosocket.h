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

struct IP4Endpoint
{
    IP4Endpoint(const IP4Address& a = IP4Address(), uint16_t p = 0 ) 
        : addr(a), port(p) {}

    bool operator == (const IP4Endpoint& e) const
    {
        return addr.value == e.addr.value && port == e.port; 
    }

    IP4Address addr;
    uint16_t port;
};

struct IncomingConnection
{
    FileHandle handle;
    IP4Endpoint endpoint;
};

struct IoVec 
{
    char* data;
    std::size_t len;
};

class Connection
{
    FileHandle d_handle;
    ContextPoll d_poll;
    IP4Endpoint d_remoteAddr;
public:
    Connection(int fd = -1);

    void attach(int fd);
    void setRemoteAddress(const IP4Endpoint& ep) { d_remoteAddr = ep; }

    // accessors
    IP4Endpoint remoteAddress() const { return d_remoteAddr; }

    // returns 0 if success, error otherwise
    int connect(const IP4Endpoint& endpoint);
    int read(char* buf, std::size_t sz);
    int writeAll(const char* buf, std::size_t sz);
    int writeAll(IoVec* buf, std::size_t count);
    void shutdown();

    // noncopyable
    Connection(const Connection&) = delete;
    Connection& operator = (const Connection&) = delete;

};

class Listener
{
    FileHandle d_handle;
    ContextPoll d_poll;
public:

    Listener();

    int bind(const IP4Endpoint& endpoint);
    int listen(int backlog);
    bool accept(Connection& conn);
};


}

