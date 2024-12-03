#  minimum required cmake version: 3.1.0...3.10
cmake_minimum_required(VERSION 3.1.0...3.10)

project(LIPSedgeAE470Reboot)

add_executable(lips-ae470-reboot ae470-reboot.cpp)
set_property(TARGET lips-ae470-reboot PROPERTY CXX_STANDARD 11)

add_dependencies(lips-ae470-reboot libcurl)
target_link_libraries(lips-ae470-reboot curl backend-ethernet)
target_include_directories(lips-ae470-reboot PRIVATE ../../third-party ../../include)

set_target_properties (lips-ae470-reboot PROPERTIES FOLDER "Tools")

install(TARGETS lips-ae470-reboot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
