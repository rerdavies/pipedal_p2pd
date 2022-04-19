# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)

include(CTest)

add_library(cotask STATIC

./cotask/Os.h
./cotask/CoService.h
./cotask/CoFile.h
./cotask/MessageAwaiter.h
./cotask/Log.h
./cotask/AsyncIo.h
./cotask/Fifo.h
./cotask/CoExec.h
./cotask/CoEvent.h

./CoTaskSchedulerPool.cpp

./ss.h
./CoTask.cpp
./Log.cpp
./CoEvent.cpp
./CoTaskSchedulerPool.h
./CoService.cpp
./CoTaskTest.cpp
./AsyncIoLinux.cpp
./CoFile.cpp
./CoExec.cpp
./OsLinux.cpp

)


target_include_directories(cotask PRIVATE "common" )

target_link_libraries(cotask PUBLIC pthread  uuid)


add_executable(taskTest
    TaskTest.cpp
    ./AsanOptions.cpp
    )

target_link_libraries(taskTest cotask)

add_test(NAME TaskTest COMMAND taskTest)



add_executable(coTaskTest
    CoTaskTest.cpp
    ./AsanOptions.cpp
)


target_link_libraries(coTaskTest pthread cotask)

add_test(NAME CoTaskTest COMMAND coTaskTest)


add_executable(coServiceTest
    CoServiceTest.cpp
    ./AsanOptions.cpp

)

target_link_libraries(coServiceTest pthread cotask)

add_test(NAME CoServiceTest COMMAND coServiceTest)

add_executable(asyncIoTest
    AsyncIoTest.cpp
    ./AsanOptions.cpp

)


target_link_libraries(asyncIoTest pthread cotask)

add_test(NAME AsyncIoTest COMMAND asyncIoTest)

add_executable(asyncExecTest
    CoExecTest.cpp
)
target_link_libraries(asyncExecTest pthread cotask)

add_test(NAME AsyncExecTest COMMAND asyncExecTest)



add_executable(coEventTest
    CoEventTest.cpp
    ./AsanOptions.cpp

)

target_link_libraries(coEventTest pthread cotask)

add_test(NAME CoEventTest COMMAND coEventTest)


add_executable(shutdownTest
    ShutdownTest.cpp
    ./AsanOptions.cpp

)

target_link_libraries(shutdownTest pthread cotask)

add_test(NAME ShutdownTest COMMAND shutdownTest)

# test_memcheck target: run valgrind memcheck
add_custom_target(test_memcheck
    COMMAND ${CMAKE_CTEST_COMMAND} 
        --force-new-ctest-process --test-action memcheck
    COMMAND cat "${CMAKE_BINARY_DIR}/Testing/Temporary/MemoryChecker.*.log"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")