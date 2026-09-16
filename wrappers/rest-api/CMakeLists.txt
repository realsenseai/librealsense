# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# BUILD_REST_API: build the React viewer into wrappers/rest-api/static so the FastAPI server
# serves it from one port, and install the server's Python dependencies into its venv.
# Requires Node.js 18+ (npm) and Python 3.10+; builds pyrealsense2 first when
# BUILD_PYTHON_BINDINGS is on so the server picks the repo build (main.py prefers it).
cmake_minimum_required(VERSION 3.10)
project(RealSenseRestApi NONE)

find_program(NPM_EXECUTABLE NAMES npm.cmd npm)  # npm.cmd first: on Windows the bare "npm" is a shell script
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "BUILD_REST_API needs npm (Node.js 18+) on the PATH")
endif()
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "BUILD_REST_API needs a Python 3 interpreter")
endif()

set(REST_API_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VIEWER_DIR ${REST_API_DIR}/tools/react-viewer)
set(VIEWER_STAMP ${CMAKE_CURRENT_BINARY_DIR}/react-viewer.stamp)
set(VENV_DIR ${REST_API_DIR}/venv)
if(WIN32)
    set(VENV_PYTHON ${VENV_DIR}/Scripts/python.exe)
else()
    set(VENV_PYTHON ${VENV_DIR}/bin/python)
endif()

file(GLOB_RECURSE VIEWER_SOURCES CONFIGURE_DEPENDS ${VIEWER_DIR}/src/*.ts ${VIEWER_DIR}/src/*.tsx ${VIEWER_DIR}/src/*.css)

# The viewer bundle: npm ci (only when package-lock changed), tsc + vite build, copy to static/
add_custom_command(
    OUTPUT ${VIEWER_STAMP}
    COMMAND ${NPM_EXECUTABLE} ci --no-audit --no-fund
    COMMAND ${NPM_EXECUTABLE} run build
    COMMAND ${CMAKE_COMMAND} -E touch ${VIEWER_STAMP}
    WORKING_DIRECTORY ${VIEWER_DIR}
    DEPENDS ${VIEWER_SOURCES} ${VIEWER_DIR}/package.json ${VIEWER_DIR}/package-lock.json ${VIEWER_DIR}/index.html ${VIEWER_DIR}/vite.config.ts
    COMMENT "Building the React viewer into wrappers/rest-api/static"
    VERBATIM
)
add_custom_target(rest-api-viewer ALL DEPENDS ${VIEWER_STAMP})

# The server's Python environment: venv + requirements (idempotent, so not part of ALL)
add_custom_target(rest-api-venv
    COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR}
    COMMAND ${VENV_PYTHON} -m pip install --quiet --upgrade pip
    COMMAND ${VENV_PYTHON} -m pip install --quiet -r ${REST_API_DIR}/requirements.txt
    WORKING_DIRECTORY ${REST_API_DIR}
    COMMENT "Creating the REST API server's Python environment"
    VERBATIM
)

# Everything the server needs to run from a fresh checkout
add_custom_target(rest-api DEPENDS rest-api-viewer rest-api-venv)
if(BUILD_PYTHON_BINDINGS AND TARGET pyrealsense2)
    add_dependencies(rest-api pyrealsense2)
endif()

install(DIRECTORY ${REST_API_DIR}/app ${REST_API_DIR}/static
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/realsense2/rest-api OPTIONAL
        PATTERN "__pycache__" EXCLUDE)
install(FILES ${REST_API_DIR}/main.py ${REST_API_DIR}/config.py ${REST_API_DIR}/requirements.txt ${REST_API_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/realsense2/rest-api)
