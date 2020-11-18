# Copyright (c) 2020 The Orbit Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

project(OrbitInstrumentation)

add_executable(OrbitInstrumentation main.cpp)

target_compile_options(OrbitInstrumentation PRIVATE 
        ${STRICT_COMPILE_FLAGS})

target_include_directories(OrbitInstrumentation PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR})

target_sources(OrbitInstrumentation PRIVATE
        main.cpp)

target_link_libraries(OrbitInstrumentation PUBLIC 
        OrbitBase
        OrbitLinuxTracing)

