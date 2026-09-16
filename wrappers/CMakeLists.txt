cmake_minimum_required(VERSION 3.10)

project(RealsenseWrappers)

set(DEPENDENCIES realsense2)

if (BUILD_PYTHON_BINDINGS OR BUILD_PYTHON_DOCS)
    if (NOT INTERNET_CONNECTION)
        message(WARNING "No internet connection. Cloning Python bindings may fail")
    endif()
    add_subdirectory(python)

endif()


if (BUILD_CV_EXAMPLES)
  add_subdirectory(opencv)
endif()

if (BUILD_DLIB_EXAMPLES)
  add_subdirectory(dlib)
endif()

if (BUILD_OPENVINO_EXAMPLES)
  add_subdirectory(openvino)
endif()

if (BUILD_OPEN3D_EXAMPLES)
  add_subdirectory(open3d)
endif()

if(BUILD_MATLAB_BINDINGS)
    add_subdirectory(matlab)
endif()

if (BUILD_PCL_EXAMPLES)
  add_subdirectory(pcl)
endif()

if(BUILD_REST_API)
  add_subdirectory(rest-api)
endif()

if(BUILD_UNITY_BINDINGS)
	if(NOT WIN32)
		message( FATAL_ERROR "BUILD_UNITY_BINDINGS is supported only for Windows OS" )
	endif()
	if(NOT BUILD_CSHARP_BINDINGS)
		message( WARNING "Enabling BUILD_CSHARP_BINDINGS and set DOTNET_VERSION_LIBRARY to 3.5 since BUILD_UNITY_BINDINGS is enabled" )
	endif()
	SET(BUILD_CSHARP_BINDINGS ON)
	SET(DOTNET_VERSION_LIBRARY 3.5)
endif()

if(BUILD_CSHARP_BINDINGS)
	if(NOT WIN32)
		message( WARNING "BUILD_CSHARP_BINDINGS is supported only for Windows OS" )
	endif()
	if(NOT BUILD_SHARED_LIBS)
		message( FATAL_ERROR "BUILD_SHARED_LIBS flag must be enabled when generating CSharp bindings" )
	endif()
    add_subdirectory(csharp)
endif()

if(BUILD_OPENNI2_BINDINGS)
	add_subdirectory(openni2)
endif()

if(BUILD_PC_STITCHING)
	add_subdirectory(pointcloud/pointcloud-stitching)
endif()
