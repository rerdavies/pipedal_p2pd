# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)

include(CTest)

add_library(cotask SHARED

./cotask/WpaConstants.h
./cotask/CoTask.h
./cotask/WpaEvent.h
./cotask/Os.h
./cotask/CoService.h
./cotask/CoFile.h
./cotask/WpaMessages.h
./cotask/MessageAwaiter.h
./cotask/Log.h
./cotask/AsyncIo.h
./cotask/Fifo.h
./cotask/WpaCliWrapper.h
./cotask/CoExec.h
./cotask/CoEvent.h

./CoTaskSchedulerPool.cpp

./ss.h
./CoTask.cpp
./CoEvent.cpp
./WpaEvent.cpp
./CoTaskSchedulerPool.h
./CoService.cpp
./CoTaskTest.cpp
./AsyncIoLinux.cpp
./CoFile.cpp
./SessionManager.cpp
./WpaMessages.cpp
./WpaCliWrapper.cpp
./CoExec.cpp
./OsLinux.cpp

)


target_include_directories(cotask PRIVATE "common" )

target_link_libraries(cotask PRIVATE pthread)


add_executable(taskTest
    TaskTest.cpp
)

target_link_libraries(taskTest pthread cotask)

add_test(NAME TaskTest COMMAND taskTest)



add_executable(coTaskTest
    CoTaskTest.cpp
)


target_link_libraries(coTaskTest pthread cotask)

add_test(NAME CoTaskTest COMMAND coTaskTest)


add_executable(coServiceTest
    CoServiceTest.cpp
)

target_link_libraries(coServiceTest pthread cotask)

add_test(NAME CoServiceTest COMMAND coServiceTest)

add_executable(asyncIoTest
    AsyncIoTest.cpp
)


target_link_libraries(asyncIoTest pthread cotask)

add_test(NAME AsyncIoTest COMMAND asyncIoTest)

add_executable(asyncExecTest
    CoExecTest.cpp
)
target_link_libraries(asyncExecTest pthread cotask)

add_test(NAME AsyncExecTest COMMAND asyncExecTest)


add_executable(wpaCliTest
    WpaCliTest.cpp
)

target_link_libraries(wpaCliTest pthread cotask)


add_executable(coEventTest
    CoEventTest.cpp
)

target_link_libraries(coEventTest pthread cotask)

add_test(NAME CoEventTest COMMAND coEventTest)


add_executable(shutdownTest
    ShutdownTest.cpp
)

target_link_libraries(shutdownTest pthread cotask)

add_test(NAME ShutdownTest COMMAND shutdownTest)

# test_memcheck target: run valgrind memcheck
add_custom_target(test_memcheck
    COMMAND ${CMAKE_CTEST_COMMAND} 
        --force-new-ctest-process --test-action memcheck
    COMMAND cat "${CMAKE_BINARY_DIR}/Testing/Temporary/MemoryChecker.*.log"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")