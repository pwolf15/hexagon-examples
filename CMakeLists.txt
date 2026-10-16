cmake_minimum_required(VERSION 3.14)

project(vecadd CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(HEXAGON_SDK_ROOT)
    include(${HEXAGON_SDK_ROOT}/build/cmake/hexagon_fun.cmake)
endif()

set(common_incs
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${HEXAGON_SDK_ROOT}/incs
    ${HEXAGON_SDK_ROOT}/incs/stddef
    ${HEXAGON_SDK_ROOT}/libs/worker_pool/inc
)

set(NO_QURT_INC 1)

if(${OS_TYPE} MATCHES "HLOS")

    add_library(vecadd SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/vecadd_stub
    )

    build_idl(inc/vecadd.idl vecadd)

    # Two host binaries from the same source — A/B test for the FastRPC
    # bounce copy. Same code, one compile-time switch.
    add_executable(vecadd_app        host/main.cpp)
    add_executable(vecadd_app_malloc host/main.cpp)
    target_compile_definitions(vecadd_app        PRIVATE USE_RPCMEM=1)
    target_compile_definitions(vecadd_app_malloc PRIVATE USE_RPCMEM=0)

    set(host_apps vecadd_app vecadd_app_malloc)

    set_target_properties(vecadd PROPERTIES LINK_FLAGS "-Wl,-soname,libvecadd.so")
    foreach(app IN LISTS host_apps)
        target_link_options(${app} PRIVATE "-Wl,-rpath,$ORIGIN")
    endforeach()

    target_include_directories(vecadd PRIVATE ${common_incs})

    choose_dsprpc(${DSP_TYPE} dsprpc)
    link_custom_library(vecadd ${dsprpc})

    foreach(app IN LISTS host_apps)
        target_include_directories(${app} PRIVATE ${common_incs})
        link_custom_library(${app} ${dsprpc})
        # SDK helper: adds ipc/fastrpc/rpcmem/inc and links the prebuilt
        # rpcmem lib for the current HLOS target. Linked into both apps;
        # the malloc variant just never calls it.
        link_custom_library(${app} rpcmem)
    endforeach()

    if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        foreach(app IN LISTS host_apps)
            target_include_directories(${app} PRIVATE
                ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1
            )
            target_link_libraries(${app} vecadd c++_shared)
        endforeach()
    else()
        target_link_directories(vecadd PRIVATE
            ${HEXAGON_SDK_ROOT}/ipc/fastrpc/remote/ship/UbuntuARM_aarch64
        )
        target_link_libraries(vecadd cdsprpc)
        foreach(app IN LISTS host_apps)
            target_link_directories(${app} PRIVATE
                ${HEXAGON_SDK_ROOT}/ipc/fastrpc/remote/ship/UbuntuARM_aarch64
            )
            target_link_libraries(${app} vecadd cdsprpc)
        endforeach()
    endif()

    copy_binaries(vecadd_app vecadd_app_malloc vecadd)

else()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -std=c++14")

    set(skel_srcs
        ${CMAKE_CURRENT_BINARY_DIR}/vecadd_skel
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/vecadd_imp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/vecmul_imp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/version.c
        ${HEXAGON_SDK_ROOT}/libs/worker_pool/src/worker_pool.cpp
    )

    add_library(vecadd_skel_OBJS OBJECT ${skel_srcs})


        # link_custom_library(vecadd_skel_OBJS worker_pool_static)

    target_compile_options(vecadd_skel_OBJS PRIVATE
        -mhvx-ieee-fp
        -mhvx-qfloat
    )

    target_include_directories(vecadd_skel_OBJS PRIVATE
        ${common_incs}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    build_idl(inc/vecadd.idl vecadd_skel_OBJS)

    add_library(vecadd_skel SHARED
        $<TARGET_OBJECTS:vecadd_skel_OBJS>
    )

    target_include_directories(vecadd_skel PRIVATE
        ${common_incs}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(vecadd_skel
        ${CMAKE_CURRENT_SOURCE_DIR}/rust-kernel/target/hexagon-unknown-linux-musl/release/librust_kernel.a
        ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++abi.so.1
        ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++.so.1
    )

    file(COPY ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++abi.so.1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++.so.1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++abi.so.1.0 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${HEXAGON_LIB_DIR}/${HEXAGON_ARCH}/G0/pic/libc++.so.1.0 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    if(EXISTS "${HEXAGON_SDK_ROOT}/libs/weak_refs/ship/hexagon_toolv84/weak_refs.a")
        target_link_libraries(vecadd_skel
            ${HEXAGON_SDK_ROOT}/libs/weak_refs/ship/hexagon_toolv84/weak_refs.a
        )
    endif()

    copy_binaries(vecadd_skel)

endif()