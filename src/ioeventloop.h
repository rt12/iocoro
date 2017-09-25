#include "iocommon.h"
#include <stdint.h>

namespace iocoro
{

class FilePoll
{
    int d_fd;
    uint32_t d_events;
};

class Poller
{
    FileHandle d_efd;
public:
    void add(FilePoll* pd);
    void remove(FilePoll* pd);
    void wait(int timeoutMs);
};

}

