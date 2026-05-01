cmake_minimum_required(VERSION 3.15)
project(bfc_tunnel LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pin to a specific upstream revision (see git ls-remote ... HEAD).
set(BFC_GIT_REPOSITORY "https://github.com/therooftopprinz/bfc.git")
set(BFC_GIT_TAG "bd5e0cce68e8aea8adc8e995b1ad23361e425d29")

# GoogleTest — pin to main HEAD (see git ls-remote https://github.com/google/googletest.git HEAD).
set(GOOGLETEST_GIT_REPOSITORY "https://github.com/google/googletest.git")
set(GOOGLETEST_GIT_TAG "d72f9c8aea6817cdf1ca0ac10887f328de7f3da2")

include(FetchContent)
FetchContent_Declare(
  bfc
  GIT_REPOSITORY "${BFC_GIT_REPOSITORY}"
  GIT_TAG "${BFC_GIT_TAG}"
)
FetchContent_GetProperties(bfc)
if(NOT bfc_POPULATED)
  FetchContent_Populate(bfc)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY "${GOOGLETEST_GIT_REPOSITORY}"
  GIT_TAG "${GOOGLETEST_GIT_TAG}"
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(bfc_headers INTERFACE)
target_include_directories(bfc_headers INTERFACE "${bfc_SOURCE_DIR}/src")
add_library(bfc::bfc ALIAS bfc_headers)

set(BTPROTOCOL_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/interface/btprotocol.cum")
set(BTPROTOCOL_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/bfc_tunnel/protocol")
set(BTPROTOCOL_OUTPUT "${BTPROTOCOL_OUTPUT_DIR}/btprotocol.hpp")
set(BTPROTOCOL_GENERATOR "" CACHE STRING "Generator command or absolute path for btprotocol.cum (e.g. cum)")

if(BTPROTOCOL_GENERATOR)
  if(IS_ABSOLUTE "${BTPROTOCOL_GENERATOR}")
    set(BTPROTOCOL_GENERATOR_EXECUTABLE "${BTPROTOCOL_GENERATOR}")
  else()
    find_program(BTPROTOCOL_GENERATOR_EXECUTABLE NAMES "${BTPROTOCOL_GENERATOR}")
  endif()
else()
  find_program(BTPROTOCOL_GENERATOR_EXECUTABLE NAMES cum)
endif()

if(NOT BTPROTOCOL_GENERATOR_EXECUTABLE)
  message(FATAL_ERROR
    "Could not find btprotocol generator executable.\n"
    "Install it or configure CMake with:\n"
    "  -DBTPROTOCOL_GENERATOR=/absolute/path/to/generator\n"
    "or\n"
    "  -DBTPROTOCOL_GENERATOR=<command-in-PATH>")
endif()

add_custom_command(
  OUTPUT "${BTPROTOCOL_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BTPROTOCOL_OUTPUT_DIR}"
  COMMAND "${BTPROTOCOL_GENERATOR_EXECUTABLE}" "${BTPROTOCOL_INPUT}" "${BTPROTOCOL_OUTPUT}"
  DEPENDS "${BTPROTOCOL_INPUT}"
  COMMENT "Generating btprotocol.hpp from btprotocol.cum"
  VERBATIM
)

add_custom_target(generate_btprotocol DEPENDS "${BTPROTOCOL_OUTPUT}")

add_library(bfc_tunnel STATIC
  src/bfc_tunnel/transport/udp_unicast_transport.cpp
  src/bfc_tunnel/utils/logger.cpp
  src/bfc_tunnel/node.cpp
  src/bfc_tunnel/route_manager.cpp
)
add_dependencies(bfc_tunnel generate_btprotocol)
target_sources(bfc_tunnel PRIVATE "${BTPROTOCOL_OUTPUT}")
target_include_directories(bfc_tunnel PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(bfc_tunnel PUBLIC bfc_headers)

set(BFC_TUNNEL_COMPILE_WARNINGS
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wformat=2>)
target_compile_options(bfc_tunnel PRIVATE ${BFC_TUNNEL_COMPILE_WARNINGS})

enable_testing()

add_executable(bfc_tunnel_tests
  src/tests/protocol_tests.cpp
  src/tests/gtestmain.cpp
)
target_link_libraries(bfc_tunnel_tests PRIVATE GTest::gtest bfc_tunnel)
target_compile_options(bfc_tunnel_tests PRIVATE ${BFC_TUNNEL_COMPILE_WARNINGS})
add_test(NAME bfc_tunnel_tests COMMAND bfc_tunnel_tests)
