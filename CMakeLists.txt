cmake_minimum_required(VERSION 3.14)

project(vecadd CXX C ASM)

if(HEXAGON_SDK_ROOT)
    include(${HEXAGON_SDK_ROOT}/build/cmake/hexagon_fun.cmake)
endif()

set(common_incs
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${HEXAGON_SDK_ROOT}/incs
    ${HEXAGON_SDK_ROOT}/incs/stddef
)

set(NO_QURT_INC 1)

if(${OS_TYPE} MATCHES "HLOS")

    add_library(vecadd SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/vecadd_stub
    )

    build_idl(inc/vecadd.idl vecadd)

    # Host executable
    add_executable(vecadd_app
        host/main.cpp
    )

    target_include_directories(vecadd PRIVATE ${common_incs})
    target_include_directories(vecadd_app PRIVATE
        ${common_incs}
        ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1
    )

    choose_dsprpc(${DSP_TYPE} dsprpc)

    link_custom_library(vecadd ${dsprpc})
    link_custom_library(vecadd_app ${dsprpc})

    target_link_libraries(vecadd_app vecadd c++_shared)

    copy_binaries(vecadd_app vecadd)

else()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -std=c++14")

    set(skel_srcs
        ${CMAKE_CURRENT_BINARY_DIR}/vecadd_skel
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/vecadd_imp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/version.c
    )

    add_library(vecadd_skel_OBJS OBJECT ${skel_srcs})

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