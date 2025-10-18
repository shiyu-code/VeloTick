# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-build"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/tmp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/src/libuv-populate-stamp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/src/libuv-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/src/libuv-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/libuv-subbuild/libuv-populate-prefix/src/libuv-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
