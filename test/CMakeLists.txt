# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(iocorotest iocorotest.cpp iosockettest.cpp)
add_executable(perftest perftest.cpp)

target_link_libraries(iocorotest iocoro ${Boost_LIBRARIES} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)
target_link_libraries(perftest iocoro ${Boost_LIBRARIES} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)

add_test(Main iocorotest)

