#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(rs-auto-exposure)

add_executable( ${PROJECT_NAME} rs-auto-exposure.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})
set_target_properties (${PROJECT_NAME} PROPERTIES
        FOLDER "Examples/OpenCV"
)

install(
	TARGETS

	${PROJECT_NAME}

	RUNTIME DESTINATION
	${CMAKE_INSTALL_PREFIX}/bin
)
