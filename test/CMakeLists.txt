cmake_minimum_required(VERSION 3.22)

project(AudioPluginTest)

enable_testing()

set(SOURCE_FILES source/AudioProcessorTest.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${GOOGLETEST_SOURCE_DIR}/googletest/include)

target_link_libraries(${PROJECT_NAME} PRIVATE AudioPlugin GTest::gtest_main)

set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS_CXX}")

include(GoogleTest)

if(CMAKE_GENERATOR STREQUAL Xcode)
  gtest_discover_tests(${PROJECT_NAME} DISCOVERY_MODE PRE_TEST)
else()
  gtest_discover_tests(${PROJECT_NAME})
endif()
