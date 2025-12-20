#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(RealSenseRegistrationExample)

add_executable(rs-registration rs-registration.cpp)
set_property(TARGET rs-registration PROPERTY CXX_STANDARD 11)
target_link_libraries(rs-registration ${DEPENDENCIES})
set_target_properties (rs-registration PROPERTIES
	FOLDER "Examples/OpenCV"
)

install(
	TARGETS

	rs-registration

	RUNTIME DESTINATION
	${CMAKE_INSTALL_PREFIX}/bin
)
