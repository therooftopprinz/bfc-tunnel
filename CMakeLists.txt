cmake_minimum_required(VERSION 3.15)
project(bfc_tunnel LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pin to a specific upstream revision (see git ls-remote ... HEAD).
set(BFC_GIT_REPOSITORY "https://github.com/therooftopprinz/bfc.git")
set(BFC_GIT_TAG "9cdd7ba78e088fdc70cef153de2f23b7646f8452")

set(CUM_GIT_REPOSITORY "https://github.com/therooftopprinz/cum.git")
set(CUM_GIT_TAG "e0d04b66b3512e14bccfacb730dd7e3421957fe5")

# GoogleTest — pin to main HEAD (see git ls-remote https://github.com/google/googletest.git HEAD).
set(GOOGLETEST_GIT_REPOSITORY "https://github.com/google/googletest.git")
set(GOOGLETEST_GIT_TAG "d72f9c8aea6817cdf1ca0ac10887f328de7f3da2")

# Botan — pin to 3.12.0 (see git ls-remote https://github.com/randombit/botan.git refs/tags/3.12.0).
set(BOTAN_GIT_REPOSITORY "https://github.com/randombit/botan.git")
set(BOTAN_GIT_TAG "45d6f286c320b2f2efd5373d195ec88c367e3071")

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
  cum
  GIT_REPOSITORY "${CUM_GIT_REPOSITORY}"
  GIT_TAG "${CUM_GIT_TAG}"
)
FetchContent_GetProperties(cum)
if(NOT cum_POPULATED)
  FetchContent_Populate(cum)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY "${GOOGLETEST_GIT_REPOSITORY}"
  GIT_TAG "${GOOGLETEST_GIT_TAG}"
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchBotan.cmake")

add_library(bfc_headers INTERFACE)
target_include_directories(bfc_headers INTERFACE "${bfc_SOURCE_DIR}/src")
add_library(bfc::bfc ALIAS bfc_headers)

set(CUM_CPP_GEN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cum-cpp-gen.sh")

set(BTPROTOCOL_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/interface/btprotocol.cum")
set(BTPROTOCOL_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/bfc_tunnel/protocol")
set(BTPROTOCOL_OUTPUT "${BTPROTOCOL_OUTPUT_DIR}/btprotocol.hpp")
set(BTPROTOCOL_GENERATOR "" CACHE STRING "Optional override for btprotocol.hpp generator (default: cmake/cum-cpp-gen.sh)")

set(BTPROTOCOL_GENERATOR_EXECUTABLE "")
if(BTPROTOCOL_GENERATOR)
  if(IS_ABSOLUTE "${BTPROTOCOL_GENERATOR}")
    set(BTPROTOCOL_GENERATOR_EXECUTABLE "${BTPROTOCOL_GENERATOR}")
  else()
    find_program(BTPROTOCOL_GENERATOR_EXECUTABLE NAMES "${BTPROTOCOL_GENERATOR}")
  endif()
endif()

if(NOT BTPROTOCOL_GENERATOR_EXECUTABLE AND EXISTS "${CUM_CPP_GEN}")
  set(BTPROTOCOL_GENERATOR_EXECUTABLE "${CUM_CPP_GEN}")
endif()

if(NOT BTPROTOCOL_GENERATOR_EXECUTABLE)
  find_program(BTPROTOCOL_GENERATOR_EXECUTABLE NAMES cum)
endif()

if(NOT BTPROTOCOL_GENERATOR_EXECUTABLE)
  message(FATAL_ERROR
    "Could not find btprotocol generator executable.\n"
    "Expected bundled script: ${CUM_CPP_GEN}\n"
    "Install cum, ensure Python 3 is available, or configure CMake with:\n"
    "  -DBTPROTOCOL_GENERATOR=/absolute/path/to/generator\n"
    "or\n"
    "  -DBTPROTOCOL_GENERATOR=<command-in-PATH>")
endif()

if(BTPROTOCOL_GENERATOR_EXECUTABLE STREQUAL CUM_CPP_GEN)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

set(BTPROTOCOL_GENERATOR_DEPS "${BTPROTOCOL_INPUT}")
if(EXISTS "${CUM_CPP_GEN}")
  list(APPEND BTPROTOCOL_GENERATOR_DEPS "${CUM_CPP_GEN}")
endif()
if(cum_SOURCE_DIR)
  list(APPEND BTPROTOCOL_GENERATOR_DEPS
    "${cum_SOURCE_DIR}/generator/cum_to_ast.py"
    "${cum_SOURCE_DIR}/generator/ast_to_cpp.py"
    "${cum_SOURCE_DIR}/generator/ast_normalize.py"
    "${cum_SOURCE_DIR}/generator/cpp_generator.py"
  )
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BTPROTOCOL_INPUT}")

add_custom_command(
  OUTPUT "${BTPROTOCOL_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BTPROTOCOL_OUTPUT_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E env "CUM_ROOT=${cum_SOURCE_DIR}"
          "${BTPROTOCOL_GENERATOR_EXECUTABLE}" "${BTPROTOCOL_INPUT}" "${BTPROTOCOL_OUTPUT}"
  DEPENDS ${BTPROTOCOL_GENERATOR_DEPS}
  MAIN_DEPENDENCY "${BTPROTOCOL_INPUT}"
  COMMENT "Generating btprotocol.hpp from btprotocol.cum"
  VERBATIM
)

set_source_files_properties("${BTPROTOCOL_OUTPUT}" PROPERTIES GENERATED TRUE)

add_custom_target(generate_btprotocol DEPENDS "${BTPROTOCOL_OUTPUT}")

add_library(bfc_tunnel STATIC
  src/bfc_tunnel/transport/transport_types.cpp
  src/bfc_tunnel/transport/udp_unicast_transport.cpp
  src/bfc_tunnel/utils/logger.cpp
  src/bfc_tunnel/node.cpp
  src/bfc_tunnel/route_manager.cpp
)
add_dependencies(bfc_tunnel generate_btprotocol)
target_sources(bfc_tunnel PRIVATE "${BTPROTOCOL_OUTPUT}")
target_include_directories(bfc_tunnel PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${cum_SOURCE_DIR}/target_cpp"
)
target_link_libraries(bfc_tunnel PUBLIC bfc_headers botan::botan-static)

set(BFC_TUNNEL_COMPILE_WARNINGS
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wformat=2>)
target_compile_options(bfc_tunnel PRIVATE ${BFC_TUNNEL_COMPILE_WARNINGS})

enable_testing()

add_executable(bfc_tunnel_tests
  src/tests/protocol_tests.cpp
  src/tests/frame_tests.cpp
  src/tests/gtestmain.cpp
)
target_link_libraries(bfc_tunnel_tests PRIVATE GTest::gtest bfc_tunnel)
target_compile_options(bfc_tunnel_tests PRIVATE ${BFC_TUNNEL_COMPILE_WARNINGS})
add_test(NAME bfc_tunnel_tests COMMAND bfc_tunnel_tests)
