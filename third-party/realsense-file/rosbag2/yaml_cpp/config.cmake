if (NOT TARGET yaml_cpp)
    ExternalProject_Add(yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.7.0
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
endif()

ExternalProject_Get_Property(yaml_cpp SOURCE_DIR)
set(yaml_cpp_SOURCE_DIR ${SOURCE_DIR})

set(HEADER_DIR_YAML_CPP
    ${yaml_cpp_SOURCE_DIR}/include
)
