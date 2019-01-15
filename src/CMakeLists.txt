set(iocoro_sources iocoro.cpp iosocket.cpp iocommon.cpp)

if (APPLE)
    list(APPEND iocoro_sources iopoll_kqueue.cpp)
else()
    list(APPEND iocoro_sources iopoll_epoll.cpp)
endif()

add_library(iocoro ${iocoro_sources})
