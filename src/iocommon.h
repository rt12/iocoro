#pragma once

namespace iocoro
{

class FileHandle
{
    int d_fd;
public:
    FileHandle(int fd = -1) : d_fd(fd) {}
    FileHandle(FileHandle&& f);
    ~FileHandle();

    operator int () const { return d_fd; }
    int handle() const { return d_fd; }
    void close();

    FileHandle& operator = (FileHandle&& f);

    // noncopyable
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator = (const FileHandle&) = delete;
};

}

