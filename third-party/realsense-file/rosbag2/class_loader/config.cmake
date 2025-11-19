cmake_minimum_required(VERSION 3.10)

set(HEADER_FILES_CLASS_LOADER
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/class_loader.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/class_loader_core.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/exceptions.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/meta_object.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/multi_library_class_loader.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/register_macro.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/class_loader/visibility_control.hpp
)

set(SOURCE_FILES_CLASS_LOADER
    ${CMAKE_CURRENT_LIST_DIR}/src/class_loader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/class_loader_core.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/meta_object.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/multi_library_class_loader.cpp
)

set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};CLASS_LOADER_BUILDING_DLL")