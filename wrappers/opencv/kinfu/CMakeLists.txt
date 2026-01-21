cmake_minimum_required(VERSION 3.10)

project(RealSenseKinfuExample)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV 4 REQUIRED)
find_package(realsense2 REQUIRED)

set(DEPENDENCIES glfw ${OPENGL_LIBRARIES} ${DEPENDENCIES} ${OpenCV_LIBRARIES} pthread realsense2::realsense2)

add_executable(rs-kinfu rs-kinfu.cpp ../../../examples/example.hpp)
set_property(TARGET rs-kinfu PROPERTY CXX_STANDARD 11)
target_link_libraries(rs-kinfu ${DEPENDENCIES})
include_directories(../../../examples)
set_target_properties (rs-kinfu PROPERTIES
	FOLDER "Examples/OpenCV"
)

install(
	TARGETS

	rs-kinfu

	RUNTIME DESTINATION
	${CMAKE_INSTALL_PREFIX}/bin
)
