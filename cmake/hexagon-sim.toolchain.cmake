set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR hexagon)

if (NOT DEFINED ENV{HEXAGON_SDK_ROOT})
    message(FATAL_ERROR "HEXAGON_SDK_ROOT not set")
endif()
set(HEXAGON_SDK_ROOT $ENV{HEXAGON_SDK_ROOT})
file(GLOB _tools "${HEXAGON_SDK_ROOT}/tools/HEXAGON_Tools/*/Tools")
list(GET _tools 0 HEXAGON_TOOLS)

set(CMAKE_C_COMPILER ${HEXAGON_TOOLS}/bin/hexagon-clang)
set(CMAKE_CXX_COMPILER ${HEXAGON_TOOLS}/bin/hexagon-clang++)
set(HEXAGON_SIM ${HEXAGON_TOOLS}/bin/hexagon-sim)

set(HEXAGON_ARCH v68 CACHE string "DSP arch")
set(_arch_flags "-m${HEXAGON_ARCH} -mhvx -mhvx-length=128b -mhvx-ieee-fp -mhvx-qfloat")
set(CMAKE_C_FLAGS_INIT "${_arch_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_arch_flags}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)