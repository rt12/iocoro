#include "iocommon.h"
#include <stdint.h>

namespace iocoro
{

namespace EventType
{
    const uint32_t Read  = 0x01;
    const uint32_t Write = 0x02;
}

struct FilePoll
{
    FilePoll(int f) : fd(f) {}

    int fd;
    virtual void handleEvents(uint32_t events) = 0;
};

class Poller
{
    FileHandle d_efd;
    uint32_t d_count;
public:
    Poller();

    uint32_t count() const { return d_count; }
    void add(FilePoll* pd);
    void remove(FilePoll* pd);
    int wait(int timeoutMs);
};

}

