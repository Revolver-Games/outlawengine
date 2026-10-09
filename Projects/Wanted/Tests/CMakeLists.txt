# Copyright (c) Contributors to the Wanted Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT
cmake_minimum_required(VERSION 3.25)
project(WantedMissionTests LANGUAGES CXX)

enable_testing()
add_executable(WantedFirstErrandTests FirstErrandTests.cpp)
target_compile_features(WantedFirstErrandTests PRIVATE cxx_std_20)
target_include_directories(WantedFirstErrandTests PRIVATE ../Gem/Include)
if(MSVC)
    target_compile_options(WantedFirstErrandTests PRIVATE /W4 /WX)
else()
    target_compile_options(WantedFirstErrandTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME Wanted.FirstErrand COMMAND WantedFirstErrandTests)
