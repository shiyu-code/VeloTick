# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-build"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/tmp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/src/pybind11-populate-stamp"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/src"
  "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/src/pybind11-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/src/pybind11-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/05GitProject/VeloTick/VeloTick/cpp/build/_deps/pybind11-subbuild/pybind11-populate-prefix/src/pybind11-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
