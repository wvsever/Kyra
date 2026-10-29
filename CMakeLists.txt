cmake_minimum_required(VERSION 3.24)
project(kyra LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KYRA_BUILD_TESTS "Build the Kyra tests" ON)

include(cmake/product_version/ProductVersion.cmake)

# Kyra's own targets: warnings as errors, UTF-8 sources.
function(kyra_target_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /permissive- /EHsc)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endfunction()

add_subdirectory(engine)

if(KYRA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
