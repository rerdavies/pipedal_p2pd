# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)


add_executable(wpap2pd 
    main.cpp
    P2pSessionManager.h P2pSessionManager.cpp
    CommandLineParser.h
)
target_include_directories(wpap2pd INTERFACE p2psession)
target_link_libraries(wpap2pd p2psession)
target_include_directories(wpap2pd PRIVATE "../lib")



add_test(CoTaskTest coTaskTest)
