# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)

add_compile_definitions(CONFIG_CTRL_IFACE)

add_library(p2psession SHARED

./p2psession/WpaConstants.h
./p2psession/CoTask.h
./p2psession/WpaEvent.h
./p2psession/SessionLog.h
./p2psession/WpaMessages.h
./p2psession/MessageAwaiter.h
./p2psession/Log.h
./p2psession/xxxDispatcher.h
./p2psession/Fifo.h
./p2psession/P2pException.h
./p2psession/SessionManager.h
./p2psession/WpaOs.h

./common/includes.h
./common/os_unix.c
./common/common.c
./common/os.h
./common/common.h
./common/build_config.h
./common/wpa_ctrl.c
./common/wpa_ctrl.h

./WpaOsLinux.cpp
./ss.h
./CoTask.cpp
./CoTaskSchedulerPool.h
./CoTaskSchedulerPool.cpp
./SessionManager.cpp
./WpaMessages.cpp
./WpaEvent.cpp


)

target_compile_definitions(p2psession PRIVATE CONFIG_CTRL_IFACE_UNIX CONFIG_CTRL_IFACE)

target_include_directories(p2psession PRIVATE "common" )

target_link_libraries(p2psession PRIVATE pthread)


add_executable(coTaskTest
    CoTaskSchedulerPool.cpp CoTaskSchedulerPool.h
    CoTask.cpp p2psession/CoTask.h
    p2psession/Log.h
    CoTaskTest.cpp
)

target_link_libraries(coTaskTest pthread)

add_executable(coServiceTest
    CoTaskSchedulerPool.cpp CoTaskSchedulerPool.h
    CoTask.cpp p2psession/CoTask.h
    p2psession/Log.h
    p2psession/CoService.h
    CoServiceTest.cpp
)

target_link_libraries(coServiceTest pthread)


add_executable(asyncIoTest
    CoTaskSchedulerPool.cpp CoTaskSchedulerPool.h
    CoTask.cpp p2psession/CoTask.h
    AsyncFile.cpp p2psession/AsyncFile.h
    AsyncIoLinux.cpp p2psession/AsyncIo.h
    p2psession/Log.h
    AsyncIoTest.cpp
)

target_link_libraries(asyncIoTest pthread)



add_test(CoTaskTest coTaskTest)
