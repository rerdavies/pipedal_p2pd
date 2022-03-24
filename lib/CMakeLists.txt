# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)

add_compile_definitions(CONFIG_CTRL_IFACE)
add_compile_options("-fcoroutines")
add_library(p2psession SHARED
    p2psession/SessionManager.h
    p2psession/P2pException.h
    p2psession/wpa_ctrl.h
    p2psession/WpaMessages.h

    WpaMessages.cpp

    ss.h
    SessionManager.cpp
)




add_executable(coTaskTest
    CoTaskSchedulerPool.cpp CoTaskSchedulerPool.h
    CoTask.cpp p2psession/CoTask.h
    p2psession/Log.h
    CoTaskTest.cpp
)

target_link_libraries(coTaskTest pthread)




add_test(CoTaskTest coTaskTest)
