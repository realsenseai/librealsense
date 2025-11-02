cmake_minimum_required(VERSION 3.5)

set(LIBUSB_C libusb/core.c libusb/descriptor.c libusb/hotplug.c libusb/io.c libusb/strerror.c libusb/sync.c)
if(WIN32)
    list(
        APPEND
        LIBUSB_C
        libusb/os/threads_windows.c
        libusb/os/threads_windows.c
        libusb/os/events_windows.c
        libusb/os/windows_winusb.c
        libusb/os/windows_common.c
        libusb/os/windows_usbdk.c)

elseif(OB_BUILD_ANDROID)
    list(APPEND LIBUSB_C libusb/os/linux_usbfs.c libusb/os/events_posix.c libusb/os/threads_posix.c
         libusb/os/linux_netlink.c)
elseif(OB_BUILD_MACOS)
    list(APPEND LIBUSB_C libusb/os/darwin_usb.c libusb/os/events_posix.c libusb/os/threads_posix.c)
else()
    list(APPEND LIBUSB_C libusb/os/linux_usbfs.c libusb/os/events_posix.c libusb/os/threads_posix.c
         libusb/os/linux_netlink.c)
endif()

set(LIBUSB_H libusb/libusb.h)

add_library(libusb_static STATIC ${LIBUSB_C} ${LIBUSB_H})
target_include_directories(libusb_static PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libusb>
    $<INSTALL_INTERFACE:include/libusb>)
target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libusb/os)

if(OB_BUILD_LINUX)
    target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linux)
elseif(OB_BUILD_MACOS)
    target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Xcode)
    target_link_libraries(libusb_static "-framework CoreFoundation -framework IOKit -framework Security")
elseif(OB_BUILD_ANDROID)
    target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/android)
else(OB_BUILD_WIN32)
    target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/msvc)
    target_include_directories(libusb_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/msvc/getopt)
    foreach(
        flag_var
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   /utf-8")
endif()
