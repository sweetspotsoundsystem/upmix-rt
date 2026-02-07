cmake_minimum_required(VERSION 3.22)

project(UpmixRT)

set(CMAKE_CXX_STANDARD 20)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

include(cmake/cpm.cmake)

cpmaddpackage(
  NAME
  JUCE
  GIT_TAG
  8.0.6
  VERSION
  8.0.6
  GITHUB_REPOSITORY
  juce-framework/JUCE
  SOURCE_DIR
  ${LIB_DIR}/juce
)

cpmaddpackage(
  NAME
  GOOGLETEST
  GITHUB_REPOSITORY
  google/googletest
  VERSION
  1.16.0
  SOURCE_DIR
  ${LIB_DIR}/googletest
  OPTIONS
  "INSTALL_GTEST OFF"
  "gtest_force_shared_crt ON"
)

include(cmake/CompilerWarnings.cmake)
include(cmake/Util.cmake)

add_subdirectory(plugin)

enable_testing()

add_subdirectory(test)
