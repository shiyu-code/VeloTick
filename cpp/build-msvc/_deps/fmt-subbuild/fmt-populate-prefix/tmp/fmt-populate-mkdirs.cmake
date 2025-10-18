# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-build"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/tmp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/src/fmt-populate-stamp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/src/fmt-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/src/fmt-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build-msvc/_deps/fmt-subbuild/fmt-populate-prefix/src/fmt-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
