# Copyright (c) 2026 Wanted Engine contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
cmake_minimum_required(VERSION 3.22)
project(ScriptMotionStandalone VERSION 0.1.0 LANGUAGES CXX)
include(CTest)
option(SCRIPTMOTION_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(ScriptMotionCore STATIC ../Code/Source/Core/ScriptMotion.cpp ../Code/Source/Core/PoseSpace.cpp
    ../Code/Source/Core/MotionBake.cpp ../Code/Source/Core/Player.cpp)
target_include_directories(ScriptMotionCore PUBLIC ../Code/Include)
target_compile_features(ScriptMotionCore PUBLIC cxx_std_20)
set_target_properties(ScriptMotionCore PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(ScriptMotionCore PRIVATE /W4 /WX /EHs-c-)
    # MSVC's STL needs this with exceptions disabled. Propagate it to consumers:
    # mixing STL exception-class layouts causes heap corruption in Debug builds.
    target_compile_definitions(ScriptMotionCore PUBLIC _HAS_EXCEPTIONS=0)
else()
    target_compile_options(ScriptMotionCore PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-exceptions)
endif()
if(SCRIPTMOTION_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ScriptMotionCore PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ScriptMotionCore PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR "SCRIPTMOTION_SANITIZERS requires GCC or Clang")
    endif()
endif()

add_executable(scriptmotion_demo ScriptMotionDemo.cpp)
target_link_libraries(scriptmotion_demo PRIVATE ScriptMotionCore)
add_executable(scriptmotion_player PlayerDemo.cpp)
target_link_libraries(scriptmotion_player PRIVATE ScriptMotionCore)
if(BUILD_TESTING)
    set(motion_assets ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/ScriptMotion)
    foreach(clip IN ITEMS idle walk greeting)
        add_test(NAME ScriptMotionBake_${clip} COMMAND scriptmotion_demo
            ${motion_assets}/locomotion.skeleton.json ${motion_assets}/${clip}.scriptmotion.json 0.25 --bake)
    endforeach()
    add_test(NAME ScriptMotionPlaybackSequence COMMAND scriptmotion_player
        ${motion_assets}/locomotion.skeleton.json ${motion_assets}/idle.scriptmotion.json
        ${motion_assets}/walk.scriptmotion.json ${motion_assets}/greeting.scriptmotion.json)
    add_executable(ScriptMotionPlayerTests ../Code/Tests/PlayerTests.cpp)
    target_link_libraries(ScriptMotionPlayerTests PRIVATE ScriptMotionCore)
    add_test(NAME ScriptMotionPlayerTests COMMAND ScriptMotionPlayerTests)
    add_executable(ScriptMotionTests ../Code/Tests/ScriptMotionTests.cpp)
    target_link_libraries(ScriptMotionTests PRIVATE ScriptMotionCore)
    add_test(NAME ScriptMotionCoreTests COMMAND ScriptMotionTests)
    add_test(NAME ScriptMotionExample COMMAND scriptmotion_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../Examples/wanted_test_skeleton.json
        ${CMAKE_CURRENT_SOURCE_DIR}/../Examples/frontier_wave.scriptmotion.json 1.0)
    add_test(NAME ScriptMotionCourierBake COMMAND scriptmotion_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/ScriptMotion/courier.skeleton.json
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Projects/Wanted/Assets/ScriptMotion/courier_wave.scriptmotion.json 1.0 --bake)
endif()
