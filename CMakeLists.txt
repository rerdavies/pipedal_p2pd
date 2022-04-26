# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")


project(cotask VERSION 0.1.0 DESCRIPTION "P2P Session Manager for wpa_supplicant")


include(CTest)
enable_testing()

set (CMAKE_CXX_STANDARD 20)

message(STATUS "Compiler version: " CMAKE_CXX_COMPILER_VERSION)


if ((CMAKE_CXX_COMPILER_ID STREQUAL GNU) AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0))
   message(STATUS "Added -fcoroutines flag")

   add_compile_options ($<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
else()

endif()

add_compile_options("-Wall" "-Werror" "-pedantic" "-fmax-errors=50")



if (CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
message(STATUS "Address Sanitizer enabled.")

add_compile_options("-fsanitize=address")
add_link_options("-fsanitize=address")



endif()

# Include sub-projects.
add_subdirectory("lib")
add_subdirectory("doxygen")
add_subdirectory("pipedal_p2pd")


#set(CPACK_GENERATOR "DEB")
#set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin Davies <rerdavies@gmail.com>") # required
#set(CPACK_PACKAGE_VENDOR "Robin Davies")
#set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debian/package_description.txt")
#set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lv2-based guitar effects pedal for Raspberry Pi")
#set(CPACK_DEBIAN_PACKAGE_SECTION sound)
#set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
#set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "hostapd,  authbind" )
#set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
##set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
#set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local)
#set(CPACK_PROJECT_NAME ${PROJECT_NAME})
#set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
#set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

#include(CPack)
