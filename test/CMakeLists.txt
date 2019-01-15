# Locate GTest
# find_package(GTest REQUIRED)
include_directories(${gtest_SOURCE_DIR}/include)

add_executable(iocorotest iocorotest.cpp iosockettest.cpp)
add_executable(perftest perftest.cpp)

target_link_libraries(iocorotest iocoro ${Boost_LIBRARIES} gtest gtest_main pthread)
target_link_libraries(perftest iocoro ${Boost_LIBRARIES} gtest gtest_main pthread)

add_test(Main iocorotest)
