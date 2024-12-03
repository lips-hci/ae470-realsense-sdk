#  minimum required cmake version: 3.10
cmake_minimum_required(VERSION 3.10)

project(LIPSedgeAE4xxNtpSync)

add_executable(lips-ae4xx-ntpsync
    sntpd/src/sntpd.c
    sntpd/src/sntpd.h
    sntpd/src/logit.c
    sntpd/src/phaselock.c
    sntpd/src/server.c
    sntpd/config.h
    main.cpp
)
set_property(TARGET lips-ae4xx-ntpsync PROPERTY CXX_STANDARD 11)

add_dependencies(lips-ae4xx-ntpsync libcurl)
target_link_libraries(lips-ae4xx-ntpsync curl rt pthread)
target_include_directories(lips-ae4xx-ntpsync PRIVATE
    ${CMAKE_SOURCE_DIR}/third-party/rapidjson/include
    sntpd/src
    sntpd/
)

set_target_properties (lips-ae4xx-ntpsync PROPERTIES FOLDER "Tools")

install(TARGETS lips-ae4xx-ntpsync RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
