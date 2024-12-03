message(STATUS "Building external libzmq")
include(ExternalProject)

# preset c/cxx flags for static build
# flag ZMQ_STATIC is required for static build on Windows, see https://github.com/zeromq/libzmq/issues/2788
if(WIN32)
    set(ZMQ_STATIC_C_FLAGS "${CMAKE_C_FLAGS} /DZMQ_STATIC")
    set(ZMQ_STATIC_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DZMQ_STATIC")
else()
    set(ZMQ_STATIC_C_FLAGS "-DZMQ_STATIC ${CMAKE_C_FLAGS} -Wno-pedantic")
    set(ZMQ_STATIC_CXX_FLAGS "-DZMQ_STATIC ${CMAKE_CXX_FLAGS} -Wno-pedantic")
endif()

ExternalProject_Add(
    libzmq
    PREFIX libzmq-prefix

    GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
    GIT_TAG "v4.3.5"
    GIT_SHALLOW true

    #UPDATE_ALWAYS true
    #BUILD_ALWAYS true

    #UPDATE_COMMAND ${GIT_EXECUTABLE} pull && ${GIT_EXECUTABLE} reset --hard
    UPDATE_COMMAND ""
    PATCH_COMMAND ""

    SOURCE_DIR "third-party/libzmq/"
    CMAKE_ARGS -DCMAKE_CXX_STANDARD_LIBRARIES=${CMAKE_CXX_STANDARD_LIBRARIES}
            -DCMAKE_C_FLAGS=${ZMQ_STATIC_C_FLAGS}
            -DCMAKE_CXX_FLAGS=${ZMQ_STATIC_CXX_FLAGS}
            -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
            -DCMAKE_CXX_FLAGS_MINSIZEREL=${CMAKE_CXX_FLAGS_MINSIZEREL}
            -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libzmq_install
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            -DWITH_DOC=OFF
            -DWITH_DOCS=OFF
            -DWITH_PERF_TOOL=OFF
            -DBUILD_TESTS=OFF
            -DBUILD_SHARED=OFF
            -DBUILD_STATIC=ON
    TEST_COMMAND ""
)
set(LIBZMQ_LIBRARY_DIRS ${CMAKE_CURRENT_BINARY_DIR}/libzmq_install/lib)
set(LIBZMQ_LOCAL_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/libzmq_install/include)

if(WIN32)
    # link to static version: libzmq-v141-mt-s-4_3_5.lib
    #   debug static version: libzmq-v141-mt-sgd-4_3_5.lib
    #   shared lib version  : libzmq-v141-mt-4_3_5.lib
    # debug shared version  : libzmq-v141-mt-gd-4_3_5.lib
    #use generator expression to tell libzmq name
    set(LIBZMQ_LIBRARIES ${LIBZMQ_LIBRARY_DIRS}/libzmq-${CMAKE_VS_PLATFORM_TOOLSET}-mt-s$<$<CONFIG:Debug>:gd>-4_3_5${CMAKE_STATIC_LIBRARY_SUFFIX})
    #set(LIBZMQ_LIBRARIES_DEBUG ${LIBZMQ_LIBRARY_DIRS}/libzmq-${CMAKE_VS_PLATFORM_TOOLSET}-mt-sgd-4_3_5${CMAKE_STATIC_LIBRARY_SUFFIX})
    #set(LIBZMQ_LIBRARIES_RELEASE ${LIBZMQ_LIBRARY_DIRS}/libzmq-${CMAKE_VS_PLATFORM_TOOLSET}-mt-s-4_3_5${CMAKE_STATIC_LIBRARY_SUFFIX})
elseif(UNIX)
    set(LIBZMQ_LIBRARIES ${LIBZMQ_LIBRARY_DIRS}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    message(FATAL_ERROR "Have no idea how to find libzmq libs on this platform ${CMAKE_SYSTEM_NAME}")
endif()
message(STATUS "LIBZMQ_LIBRARIES := ${LIBZMQ_LIBRARIES}")