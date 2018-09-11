######### Project Setup #########

cmake_minimum_required(VERSION 3.11)
project(vm CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(source)

function(apply_compiler_settings target)
	target_compile_options("${target}" PRIVATE -Wno-sign-compare -Wno-parentheses -Wall -fno-strict-aliasing -std=c++17)
endfunction()

##########   VMConfig  ##########

file(GLOB_RECURSE SOURCE_FILES source/*.cpp source/**/*.cpp source/*.hpp source/**/*.hpp source/*.s)
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp")
add_executable(vm source/main.cpp ${SOURCE_FILES})
apply_compiler_settings(vm)

########## Test Config ##########

# Include catch
# see https://github.com/philsquared/Catch/blob/master/docs/build-systems.md
#include(ExternalProject)
#set(CATCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/lib/catch)
#ExternalProject_Add(catch SOURCE_DIR ${CATCH_DIR})
#ExternalProject_Get_Property(catch source_dir)

# Ensure that the single_include is included (not other header files)
#set(CATCH_INCLUDE_DIR ${source_dir}/single_include CACHE INTERNAL "Path to include folder for Catch")

#add_library(Catch INTERFACE)
#target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})

include_directories(lib)
include_directories(test)

# Create test executable
#set(TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/test)
#file(GLOB_RECURSE TEST_SOURCES test/*.cpp test/*.hpp test/jit/*.cpp test/jit/*.hpp)
set(TEST_SOURCES
		test/main.cpp
#		test/bytecode/bytecode.cpp
#		test/jit/CodeHeap.cpp
#		test/jit/LifetimeAnalysis.cpp
#		test/jit/SlotAllocation.cpp
#		test/generate.hpp
#		test/generate.cpp
#		test/lookups.hpp
#		test/lookups.cpp
#		test/jit/CodeBuilder.cpp
		test/jit/allocator/RegisterAllocator.cpp
		test/jit/allocator/TwoRegArchitecture.hpp
)

add_executable(tests ${TEST_SOURCES} ${SOURCE_FILES})
apply_compiler_settings(tests)

# allow unit tests to circumvent 'private' and 'protected'
target_compile_options(tests PRIVATE -fno-access-control)
target_compile_definitions(tests PUBLIC TESTING)

#target_link_libraries(tests Catch)
target_link_libraries(tests stdc++)

# Set up tests
enable_testing(true)  # Enables unit-testing.
add_test(unit tests --success --reporter foobar)

# Mock DEPENDS for tests (see https://cmake.org/Wiki/CMakeEmulateMakeCheck)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(check tests)
