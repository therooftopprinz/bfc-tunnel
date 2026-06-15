# Fetch and build Botan (Python configure.py + make; no upstream CMake target).
include(FetchContent)
include(ExternalProject)
include(ProcessorCount)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT CMAKE_MAKE_PROGRAM)
  message(FATAL_ERROR "CMAKE_MAKE_PROGRAM is not set; Botan requires make/nmake.")
endif()

if(NOT BOTAN_GIT_REPOSITORY OR NOT BOTAN_GIT_TAG)
  message(FATAL_ERROR "BOTAN_GIT_REPOSITORY and BOTAN_GIT_TAG must be set before including FetchBotan.cmake")
endif()

if(NOT BOTAN_BUILD_JOBS)
  if(CMAKE_BUILD_PARALLEL_LEVEL GREATER 0)
    set(BOTAN_BUILD_JOBS "${CMAKE_BUILD_PARALLEL_LEVEL}")
  else()
    ProcessorCount(BOTAN_BUILD_JOBS)
    if(NOT BOTAN_BUILD_JOBS)
      set(BOTAN_BUILD_JOBS 1)
    endif()
  endif()
endif()
set(BOTAN_BUILD_JOBS "${BOTAN_BUILD_JOBS}" CACHE STRING "Parallel jobs for building Botan")

set(BOTAN_MAKE_PARALLEL_ARGS "")
if(NOT CMAKE_MAKE_PROGRAM MATCHES "nmake")
  list(APPEND BOTAN_MAKE_PARALLEL_ARGS "-j${BOTAN_BUILD_JOBS}")
endif()

set(BOTAN_ENABLE_MODULES "aes,chacha,x25519,ed25519,hmac,sha2_32,sha2_64,hkdf,system_rng"
    CACHE STRING "Botan modules enabled for the minimized build")

FetchContent_Declare(
  botan
  GIT_REPOSITORY "${BOTAN_GIT_REPOSITORY}"
  GIT_TAG "${BOTAN_GIT_TAG}"
)
FetchContent_GetProperties(botan)
if(NOT botan_POPULATED)
  FetchContent_Populate(botan)
endif()

set(BOTAN_INSTALL_DIR "${CMAKE_BINARY_DIR}/botan-install")
set(BOTAN_BUILD_DIR "${CMAKE_BINARY_DIR}/botan-build")
set(BOTAN_LIBRARY "${BOTAN_INSTALL_DIR}/lib/libbotan-3.a")

# Headers and the static library are produced by the external project at build time.
file(MAKE_DIRECTORY "${BOTAN_INSTALL_DIR}/include/botan-3")
file(MAKE_DIRECTORY "${BOTAN_INSTALL_DIR}/lib")

ExternalProject_Add(bfc_tunnel_botan
  SOURCE_DIR "${botan_SOURCE_DIR}"
  CONFIGURE_COMMAND
    "${Python3_EXECUTABLE}" "${botan_SOURCE_DIR}/configure.py"
    "--prefix=${BOTAN_INSTALL_DIR}"
    "--with-build-dir=${BOTAN_BUILD_DIR}"
    "--minimized-build"
    "--disable-shared-library"
    "--enable-modules=${BOTAN_ENABLE_MODULES}"
  BUILD_COMMAND
    "${CMAKE_COMMAND}" -E chdir "${BOTAN_BUILD_DIR}" "${CMAKE_MAKE_PROGRAM}" ${BOTAN_MAKE_PARALLEL_ARGS}
  INSTALL_COMMAND
    "${CMAKE_COMMAND}" -E chdir "${BOTAN_BUILD_DIR}" "${CMAKE_MAKE_PROGRAM}" ${BOTAN_MAKE_PARALLEL_ARGS} install
  BUILD_BYPRODUCTS "${BOTAN_LIBRARY}"
  USES_TERMINAL_BUILD ON
  UPDATE_COMMAND ""
)

add_library(botan::botan-static STATIC IMPORTED GLOBAL)
set_target_properties(botan::botan-static PROPERTIES
  IMPORTED_LOCATION "${BOTAN_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${BOTAN_INSTALL_DIR}/include/botan-3"
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
)
add_dependencies(botan::botan-static bfc_tunnel_botan)

find_package(Threads REQUIRED)
target_link_libraries(botan::botan-static INTERFACE Threads::Threads)
