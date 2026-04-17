cmake_minimum_required(VERSION 3.15)
project(bfc_tunnel LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pin to a specific upstream revision (see git ls-remote ... HEAD).
set(BFC_GIT_REPOSITORY "https://github.com/therooftopprinz/bfc.git")
set(BFC_GIT_TAG "d1ce0b34ee87a52309f2c8932a3e2a3db1849ee5")

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

add_library(bfc_tunnel STATIC
  src/bfc_tunnel/route_manager.cpp
)
target_include_directories(bfc_tunnel PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(bfc_tunnel PUBLIC bfc_headers)

enable_testing()

add_executable(bfc_tunnel_tests
  src/tests/protocol_tests.cpp
  src/tests/gtestmain.cpp
)
target_link_libraries(bfc_tunnel_tests PRIVATE GTest::gtest bfc_tunnel)
add_test(NAME bfc_tunnel_tests COMMAND bfc_tunnel_tests)
