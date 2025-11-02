if (NOT TARGET usb)
    # Check if local libusb exists first
    set(LOCAL_LIBUSB_DIR "${CMAKE_SOURCE_DIR}/third-party/libusb")
    
    if(EXISTS "${LOCAL_LIBUSB_DIR}/CMakeLists.txt")
        # Use local libusb from third-party/libusb
        message(STATUS "Using local libusb from third-party/libusb")
        
        # Set platform flags for the local libusb build
        if(APPLE)
            set(OB_BUILD_MACOS ON CACHE BOOL "Build for macOS" FORCE)
        elseif(WIN32)
            set(OB_BUILD_WIN32 ON CACHE BOOL "Build for Windows" FORCE)
        elseif(ANDROID)
            set(OB_BUILD_ANDROID ON CACHE BOOL "Build for Android" FORCE)
        else()
            set(OB_BUILD_LINUX ON CACHE BOOL "Build for Linux" FORCE)
        endif()
        
        # Add libusb as subdirectory
        add_subdirectory(${LOCAL_LIBUSB_DIR} ${CMAKE_BINARY_DIR}/third-party/libusb)
        
        # Create interface target that wraps the local libusb_static
        add_library(usb INTERFACE)
        target_link_libraries(usb INTERFACE libusb_static)
        
        # Add libusb_static to the export set so it can be installed
        install(TARGETS libusb_static EXPORT realsense2Targets
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin
        )
        
        set(USE_LOCAL_USB ON)
        
    else()
        # Fallback to system or external libusb
        find_library(LIBUSB_LIB usb-1.0)
        find_path(LIBUSB_INC libusb.h HINTS PATH_SUFFIXES libusb-1.0)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(usb "libusb not found; using external version" LIBUSB_LIB LIBUSB_INC)
        if (USB_FOUND AND NOT USE_EXTERNAL_USB)
            add_library(usb INTERFACE)
            target_include_directories(usb INTERFACE ${LIBUSB_INC})
            target_link_libraries(usb INTERFACE ${LIBUSB_LIB})
        else()
            include(CMake/external_libusb.cmake)
        endif()
    endif()
    
    install(TARGETS usb EXPORT realsense2Targets)
endif()
