# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.
#  minimum required cmake version: 3.1.0...3.10
cmake_minimum_required(VERSION 3.1.0...3.10)

project(LIPSedgeExamplesCaptureSafe)

if(BUILD_GRAPHICAL_EXAMPLES)
    add_executable(lips-capture-safe lips-capture-safe.cpp ../example.hpp)
    set_property(TARGET lips-capture-safe PROPERTY CXX_STANDARD 11)
    # libcurl requires build option BUILD_LIBCURL
    add_dependencies(lips-capture-safe libcurl)
    target_link_libraries(lips-capture-safe ${DEPENDENCIES} curl backend-ethernet)
    include_directories(../ ../../third-party/tclap/include ../../examples ../../third-party ../../include)
    set_target_properties (lips-capture-safe PROPERTIES FOLDER "Examples")

    install(TARGETS lips-capture-safe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
