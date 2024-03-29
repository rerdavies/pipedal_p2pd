# CMakeList.txt : Make proceduire for libp2psession.so
#
cmake_minimum_required (VERSION 3.9)

set(WPA_CTRL_PATH ../..//wpa_supplicant-2.10/src)

add_executable(pipedal_p2pd 
    main.cpp
    RestartCounter.h
    autoptr_vector.h
    WpaCtrl.cpp includes/WpaCtrl.h
    ServiceConfiguration.cpp ServiceConfiguration.hpp

    WpaEvent.cpp includes/WpaEvent.h
    WpaMessages.cpp includes/WpaMessages.h
    P2pUtil.cpp includes/P2pUtil.h
    P2pConfiguration.cpp includes/P2pConfiguration.h
    ss.h
    ConfigSerializer.h ConfigSerializer.cpp
    includes/WifiP2pDnsSdServiceInfo.h WifiP2pDnsSdServiceInfo.cpp
    includes/DnsSdTxtRecord.h
    includes/WpaExceptions.h
    WpaChannel.cpp includes/WpaChannel.h
    WpaSupplicant.cpp includes/WpaSupplicant.h
    P2pSessionManager.cpp includes/P2pSessionManager.h 
    P2pGroup.cpp includes/P2pGroup.h
    CommandLineParser.h
    DnsMasqProcess.cpp includes/DnsMasqProcess.h
    AsanOptions.cpp
)
target_compile_definitions(pipedal_p2pd PRIVATE CONFIG_CTRL_IFACE CONFIG_CTRL_IFACE_UNIX)
target_link_libraries(pipedal_p2pd cotask uuid systemd )
target_include_directories(pipedal_p2pd PRIVATE
     "../lib"
     .
     )

add_executable(referencetest 
    ReferenceTest.cpp
)

add_test(NAME ReferenceTest COMMAND referencetest)

add_executable(dnssdtest
    WifiP2pDnsSdServiceInfoTest.cpp
    P2pUtil.cpp includes/P2pUtil.h
    includes/WifiP2pDnsSdServiceInfo.h WifiP2pDnsSdServiceInfo.cpp
)

add_test(NAME DnsSdTest COMMAND dnssdtest)

#add_executable(wpaCliTest
#    WpaCliTest.cpp
#)

#target_link_libraries(wpaCliTest pthread cotask)



