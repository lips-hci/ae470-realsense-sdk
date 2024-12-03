#  minimum required cmake version: 3.1.0...3.10
cmake_minimum_required(VERSION 3.1.0...3.10)

project(LIPSedgeAE400Toolkit)

if(WIN32)
add_executable(lips-ae400-toolkit
    lips_ae400_toolkit.cpp
    win_scan.h
    win_scan.cpp
    write_html.hpp
)
else()
add_executable(lips-ae400-toolkit
    lips_ae400_toolkit.cpp
    linux_scan.h
    linux_scan.cpp
    write_html.hpp
)
endif()

set_property(TARGET lips-ae400-toolkit PROPERTY CXX_STANDARD 11)
set_target_properties (lips-ae400-toolkit PROPERTIES FOLDER "Tools")

install(TARGETS lips-ae400-toolkit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
