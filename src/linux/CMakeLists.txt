# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.
# Copyright(c) 2020-2024 LIPS Corporation. All Rights Reserved.

# Export IMU get API to our librealsense
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/ae400-imu.cpp"
)

if(WIN32)
    set(OS win)
    # Variable TARGET_ARCH can be x86, x64
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64 bits
        set(TARGET_ARCH x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32 bits
        set(TARGET_ARCH x86)
    endif()
else()
    set(OS linux)
    # Variable TARGET_ARCH can be amd64, i386, armhf, arm64
    execute_process(COMMAND bash -c "dpkg --print-architecture"
        OUTPUT_VARIABLE TARGET_ARCH
        ERROR_QUIET
    )
    string(STRIP ${TARGET_ARCH} TARGET_ARCH)
endif()

# AE430/470 uses RS 2.43.0
if(NOT ${REALSENSE_VERSION_STRING} VERSION_EQUAL 2.43)
    message(FATAL_ERROR "AE430/470 SDK requires RealSense SDK codebase v2.43.0. Please check RS2_API version in include/librealsense2/rs.h")
else()
    # AE430/470: starts from ethernet backend v1270
    set(LIBVERSION 1.2.7.1)
endif()

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH}
    ${CMAKE_SOURCE_DIR}/third-party/lips/lib/v${LIBVERSION}/${OS}
)

# Look for the library for Release
find_library(LIB_BACKEND_ETHERNET
    NAMES backend-ethernetz
    PATH_SUFFIXES ${TARGET_ARCH} ${TARGET_ARCH}/Release
)

# Look for the library for Debug
find_library(LIB_BACKEND_ETHERNET_DEBUG
    NAMES backend-ethernetz
    PATH_SUFFIXES ${TARGET_ARCH} ${TARGET_ARCH}/Debug
)

if(${LIB_BACKEND_ETHERNET} STREQUAL "LIB_BACKEND_ETHERNET-NOTFOUND")
    message(FATAL_ERROR "[LIPS PATCH] backend-ethernet library not found! Please check github.com/lips-hci")
else()
    if(NOT TARGET backend-ethernet)
        add_library(backend-ethernet STATIC IMPORTED)
    endif()
    set_target_properties(backend-ethernet PROPERTIES
        IMPORTED_LOCATION ${LIB_BACKEND_ETHERNET}
        IMPORTED_LOCATION_RELEASE ${LIB_BACKEND_ETHERNET}
        IMPORTED_LOCATION_MINSIZEREL ${LIB_BACKEND_ETHERNET}
        IMPORTED_LOCATION_RELWITHDEBINFO ${LIB_BACKEND_ETHERNET}
        IMPORTED_LOCATION_DEBUG ${LIB_BACKEND_ETHERNET_DEBUG}
    )
    message(STATUS "[LIPS PATCH] Found backend-ethernet library: ${LIB_BACKEND_ETHERNET}")
    target_link_libraries(${LRS_TARGET} PRIVATE backend-ethernet udev curl)
    mark_as_advanced(LIB_BACKEND_ETHERNET LIB_BACKEND_ETHERNET_DEBUG)
endif()

# src/libusb requires libusb header, here we include libusb for it
include(libusb_config)
target_link_libraries(${LRS_TARGET} PRIVATE usb)
if(USE_EXTERNAL_USB)
    add_dependencies(${LRS_TARGET} libusb)
endif()

# through HW monitor to get device clock time (camera FW time) can bring overhead
# we found that longer sampling interval (100ms -> 5000ms) can reduce overhead at Depth stream beginning
# impact file: src/ds5/ds5-device.cpp
#              src/global_timestamp_reader.cpp
#target_compile_definitions(${LRS_TARGET} PRIVATE DISABLE_GLOBAL_TIME)

# AE430/470 uses ZeroMQ sockets
if(BUILD_WITH_ZMQ)
    add_dependencies(${LRS_TARGET} libzmq)
    target_compile_definitions(${LRS_TARGET} PRIVATE BUILD_WITH_ZMQ)
    target_include_directories(${LRS_TARGET} PRIVATE ${LIBZMQ_LOCAL_INCLUDE_PATH})
    target_link_libraries(${LRS_TARGET} PRIVATE ${LIBZMQ_LIBRARIES})
endif()