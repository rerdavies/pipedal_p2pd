# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)


add_library(p2psession SHARED

./p2psession/WpaConstants.h
./p2psession/CoTask.h
./p2psession/WpaEvent.h
./p2psession/Os.h
./p2psession/CoService.h
./p2psession/AsyncFile.h
./p2psession/WpaMessages.h
./p2psession/MessageAwaiter.h
./p2psession/Log.h
./p2psession/AsyncIo.h
./p2psession/Fifo.h
./p2psession/P2pException.h
./p2psession/WpaCliWrapper.h
./p2psession/AsyncExec.h
./p2psession/CoEvent.h

./CoTaskSchedulerPool.cpp

./ss.h
./CoTask.cpp
./CoEvent.cpp
./WpaEvent.cpp
./CoTaskSchedulerPool.h
./CoService.cpp
./CoTaskTest.cpp
./AsyncIoLinux.cpp
./AsyncFile.cpp
./SessionManager.cpp
./WpaMessages.cpp
./WpaCliWrapper.cpp
./AsyncExec.cpp
./OsLinux.cpp

)


target_include_directories(p2psession PRIVATE "common" )

target_link_libraries(p2psession PRIVATE pthread)


add_executable(coTaskTest
    CoTaskTest.cpp
)


target_link_libraries(coTaskTest pthread p2psession)

add_test(NAME CoTaskTest COMMAND coTaskTest)


add_executable(coServiceTest
    CoServiceTest.cpp
)

target_link_libraries(coServiceTest pthread p2psession)

add_test(NAME CoServiceTest COMMAND coServiceTest)

add_executable(asyncIoTest
    AsyncIoTest.cpp
)


target_link_libraries(asyncIoTest pthread p2psession)

add_test(NAME AsyncIoTest COMMAND asyncIoTest)

add_executable(asyncExecTest
    AsyncExecTest.cpp
)
target_link_libraries(asyncExecTest pthread p2psession)

add_test(NAME AsyncExecTest COMMAND asyncExecTest)


add_executable(wpaCliTest
    WpaCliTest.cpp
)

target_link_libraries(wpaCliTest pthread p2psession)


add_executable(coEventTest
    CoEventTest.cpp
)

target_link_libraries(coEventTest pthread p2psession)

add_test(NAME CoEventTest COMMAND coEventTest)
