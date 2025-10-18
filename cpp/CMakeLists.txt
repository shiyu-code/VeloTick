# VeloTick marker
cmake_minimum_required(VERSION 3.20)
project(velotick_gateway LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: 编译源文件按 UTF-8 读取，避免中文注释导致解析错误
if (MSVC)
  add_compile_options(/utf-8)
endif()

include(FetchContent)

# Build option: run without Python embedding
option(VELO_NO_PYTHON "Build without Python embedding" ON)

# Fetch dependencies
FetchContent_Declare(pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.11.1
)
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.1.1
)
FetchContent_Declare(toml11
  GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
  GIT_TAG v3.7.1
)
FetchContent_Declare(uwebsockets
  GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
  GIT_TAG v20.32.0
)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(pybind11 fmt toml11 uwebsockets)

# Build uSockets from uWebSockets source
# Build uSockets manually and fetch libuv
FetchContent_Declare(libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG v1.48.0
)
FetchContent_MakeAvailable(libuv)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_library(usockets STATIC
  ${uwebsockets_SOURCE_DIR}/uSockets/src/context.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/loop.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/socket.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/udp.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/eventing/libuv.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/bsd.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/crypto/openssl.c
  ${uwebsockets_SOURCE_DIR}/uSockets/src/crypto/sni_tree.cpp
)

target_include_directories(usockets PUBLIC
  ${uwebsockets_SOURCE_DIR}/uSockets/src
)

# Enable OpenSSL implementation in uSockets
target_compile_definitions(usockets PUBLIC LIBUS_USE_OPENSSL)

target_link_libraries(usockets PUBLIC uv OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
if (WIN32)
  target_link_libraries(usockets PUBLIC ws2_32)
endif()

# 预留：CTP 引擎可选编译
option(VELO_USE_CTP "Enable CTP interface integration" OFF)

add_executable(velotick_gateway
  src/main.cpp
  src/md_engine.cpp
  src/tcp_server.cpp
  src/py_bridge.cpp
  src/globals.cpp
  src/ctp_engine.cpp
)

target_include_directories(velotick_gateway PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${uwebsockets_SOURCE_DIR}/src
  ${uwebsockets_SOURCE_DIR}
)

# Link libraries
# uWebSockets is header-only; link its C dependency usockets + OpenSSL/ZLIB
# fmt for logging; toml11 for config parsing

# Conditionally link Python embedding only when needed
if (NOT VELO_NO_PYTHON)
  target_link_libraries(velotick_gateway PRIVATE pybind11::embed)
endif()

target_link_libraries(velotick_gateway PRIVATE
  fmt::fmt
  toml11::toml11
  usockets
  OpenSSL::SSL
  ZLIB::ZLIB
)

# Build without Python embedding for easier web verification
# Define VELO_NO_PYTHON to strip python311.dll dependency in this build
if (VELO_NO_PYTHON)
  target_compile_definitions(velotick_gateway PRIVATE VELO_NO_PYTHON)
endif()

if (UNIX)
  target_link_libraries(velotick_gateway PRIVATE pthread)
endif()

# 可选：当启用 VELO_USE_CTP 时，预留包含/链接配置
if (VELO_USE_CTP)
  target_include_directories(velotick_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ctp/include
  )
  if (WIN32)
    target_link_directories(velotick_gateway PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../ctp/lib
    )
    # 注意：库名需与实际文件一致；此处为占位
    target_link_libraries(velotick_gateway PRIVATE thostmduserapi thosttraderapi)
  endif()
  target_compile_definitions(velotick_gateway PRIVATE VELO_USE_CTP)
endif()

# VeloTick marker