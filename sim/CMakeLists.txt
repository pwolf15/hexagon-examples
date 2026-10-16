cmake_minimum_required(VERSION 3.20)
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/hexagon-sim.toolchain.cmake)
endif()
project(hexagon_sim_tests CXX)
set(CMAKE_CXX_STANDARD 14)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

file(GLOB _qaic_dirs "${HEXAGON_SDK_ROOT}/ipc/fastrpc/qaic/*")
find_program(QAIC qaic PATHS ${_qaic_dirs} NO_DEFAULT_PATH)
if(NOT QAIC)
    message(FATAL_ERROR "qaic not found under ${HEXAGON_SDK_ROOT}/ipc/fastrpc/qaic")
endif()

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
add_custom_command(
    OUTPUT ${GEN}/vecadd.h ${GEN}/vecadd_stub.c ${GEN}/vecadd_skel.c
    COMMAND ${QAIC} -mdll -o ${GEN}
            -I ${HEXAGON_SDK_ROOT}/incs/
            -I ${HEXAGON_SDK_ROOT}/incs/stddef
            -I ${HEXAGON_SDK_ROOT}/ipc/fastrpc/incs
            ${ROOT}/inc/vecadd.idl
    DEPENDS ${ROOT}/inc/vecadd.idl
    COMMENT "qaic: vecadd.idl")
add_custom_target(vecadd_idl DEPENDS ${GEN}/vecadd.h)


add_executable(test_sim
    ${ROOT}/dsp/test_main.cpp
    ${ROOT}/dsp/vecadd_imp.cpp
    ${ROOT}/dsp/vecmul_imp.cpp
    ${ROOT}/dsp/sim_stubs.cpp)

add_dependencies(test_sim vecadd_idl)
target_include_directories(test_sim PRIVATE
    ${ROOT} ${GEN}
    ${HEXAGON_SDK_ROOT}/incs
    ${HEXAGON_SDK_ROOT}/incs/stddef
    ${HEXAGON_SDK_ROOT}/rtos/qurt/computev73/include/qurt
     ${HEXAGON_SDK_ROOT}/rtos/qurt/computev73/include/posix
    ${HEXAGON_SDK_ROOT}/libs/worker_pool/inc)

set(RUST_LIB ${ROOT}/rust-kernel/target/hexagon-unknown-linux-musl/release/librust_kernel.a)
if(EXISTS ${RUST_LIB})
    target_link_libraries(test_sim PRIVATE ${RUST_LIB})
else()
    target_compile_definitions(test_sim PRIVATE NO_RUST_KERNEL)
endif()

enable_testing()
add_test(NAME hexagon_sim COMMAND ${HEXAGON_SIM} --simulated_returnval --timing $<TARGET_FILE:test_sim>)