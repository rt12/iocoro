#include "iocommon.h"
#include <unistd.h>

namespace iocoro
{

FileHandle::FileHandle(FileHandle&& f) : d_fd(f.d_fd)
{
    f.d_fd = -1;
}

FileHandle::~FileHandle()
{
    close();
}

void FileHandle::close()
{
    if (d_fd != -1) {
        ::close(d_fd);
        d_fd = -1;
    }
}

FileHandle& FileHandle::operator = (FileHandle&& f)
{
    close();
    d_fd = f.d_fd;
    f.d_fd = -1;

    return *this;
}

}

