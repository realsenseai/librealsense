include(ExternalProject)

ExternalProject_Add(
    libusb
    PREFIX libusb
    GIT_REPOSITORY "https://github.com/libusb/libusb-cmake.git"
    GIT_TAG "v1.0.27-1" # "v1.0.27-1" 

    UPDATE_COMMAND ""
    PATCH_COMMAND ""

    SOURCE_DIR "third-party/libusb/"
    CMAKE_ARGS -DCMAKE_CXX_STANDARD_LIBRARIES=${CMAKE_CXX_STANDARD_LIBRARIES}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DANDROID_ABI=${ANDROID_ABI}
            -DANDROID_STL=${ANDROID_STL}
            -DBUILD_SHARED_LIBS=OFF
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libusb_install
            -DLIBUSB_INSTALL_TARGETS=ON
            --no-warn-unused-cli
    TEST_COMMAND ""
    #BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/libusb_install/lib/${CMAKE_STATIC_LIBRARY_PREFIX}usb${CMAKE_STATIC_LIBRARY_SUFFIX}
)

add_library(usb INTERFACE)
target_include_directories(usb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/third-party/libusb/libusb>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/third-party/libusb/libusb/libusb>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/libusb_install/include/libusb-1.0>
)
if(WIN32)
    target_link_libraries(usb INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/libusb_install/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libusb-1.0${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    target_link_libraries(usb INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/libusb_install/lib/libusb-1.0${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()
set(USE_EXTERNAL_USB ON) # INTERFACE libraries can't have real deps, so targets that link with usb need to also depend on libusb

set_target_properties( libusb PROPERTIES FOLDER "3rd Party")

if (APPLE)
  find_library(corefoundation_lib CoreFoundation)
  find_library(iokit_lib IOKit)
  find_library(security_lib Security)
  target_link_libraries(usb INTERFACE objc ${corefoundation_lib} ${iokit_lib} ${security_lib})
endif()
