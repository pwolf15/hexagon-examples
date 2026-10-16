cmake_minimum_required(VERSION 3.20)
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/hexagon-sim.toolchain.cmake)
endif()
project(hexagon_sim_tests CXX)
set(CMAKE_CXX_STANDARD 14)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(GEN  ${ROOT}/hexagon_ReleaseG_toolv19_${HEXAGON_ARCH}) # generated header from QAIC

add_executable(test_sim
    ${ROOT}/dsp/test_main.cpp
    ${ROOT}/dsp/vecadd_imp.cpp
    ${ROOT}/dsp/vecmul_imp.cpp
    ${ROOT}/dsp/sim_stubs.cpp)

target_include_directories(test_sim PRIVATE
    ${ROOT} ${GEN}
    ${HEXAGON_SDK_ROOT}/incs
    ${HEXAGON_SDK_ROOT}/incs/stddef
    ${HEXAGON_SDK_ROOT}/rtos/qurt/computev73/include/qurt
     ${HEXAGON_SDK_ROOT}/rtos/qurt/computev73/include/posix
    ${HEXAGON_SDK_ROOT}/libs/worker_pool/inc)

target_link_libraries(test_sim PRIVATE
    ${ROOT}/rust-kernel/target/hexagon-unknown-linux-musl/release/librust_kernel.a)

enable_testing()
add_test(NAME hexagon_sim COMMAND ${HEXAGON_SIM} --timing $<TARGET_FILE:test_sim>)