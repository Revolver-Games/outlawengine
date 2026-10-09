# Copyright (c) 2026 Revolver Games contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
cmake_minimum_required(VERSION 3.25)
project(CineScriptStandalone LANGUAGES CXX)
include(CTest)
set(SM ${CMAKE_CURRENT_SOURCE_DIR}/../../ScriptMotion/Code)
add_library(CineScriptCore STATIC ../Code/Source/Timeline.cpp ${SM}/Source/Core/ScriptMotion.cpp)
target_include_directories(CineScriptCore PUBLIC ../Code/Include ${SM}/Include PRIVATE ${SM}/Source/Core)
target_compile_features(CineScriptCore PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(CineScriptCore PRIVATE /W4 /WX /EHs-c-)
    target_compile_definitions(CineScriptCore PUBLIC _HAS_EXCEPTIONS=0)
else()
    target_compile_options(CineScriptCore PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-exceptions)
endif()
add_executable(cinescript_demo Demo.cpp)
target_link_libraries(cinescript_demo PRIVATE CineScriptCore)
target_include_directories(cinescript_demo PRIVATE ../../../Projects/Wanted/Gem/Include)
if(BUILD_TESTING)
    add_executable(CineScriptTests ../Code/Tests/TimelineTests.cpp)
    target_link_libraries(CineScriptTests PRIVATE CineScriptCore)
    add_test(NAME CineScript.Timeline COMMAND CineScriptTests)
    add_test(NAME CineScript.MercyCrossing COMMAND cinescript_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/CineScript/mercy_delivery.cinescript.json
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/ScriptMotion/courier.skeleton.json
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/ScriptMotion/courier_wave.scriptmotion.json)
endif()
