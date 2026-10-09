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

set(scriptmotion_code ${CMAKE_CURRENT_SOURCE_DIR}/../../../Gems/ScriptMotion/Code)
add_library(WantedNarrative STATIC ../Gem/Source/Narrative.cpp)
target_compile_features(WantedNarrative PUBLIC cxx_std_20)
target_include_directories(WantedNarrative PUBLIC ../Gem/Include ${scriptmotion_code}/Include PRIVATE ${scriptmotion_code}/Source/Core)
if(MSVC)
    target_compile_options(WantedNarrative PRIVATE /W4 /WX /EHs-c-)
    target_compile_definitions(WantedNarrative PUBLIC _HAS_EXCEPTIONS=0)
else()
    target_compile_options(WantedNarrative PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-exceptions)
endif()
add_executable(WantedNarrativeTests NarrativeTests.cpp)
target_link_libraries(WantedNarrativeTests PRIVATE WantedNarrative)
add_test(NAME Wanted.Narrative COMMAND WantedNarrativeTests
    ${CMAKE_CURRENT_SOURCE_DIR}/../Assets/Narrative/unclaimed_post.narrative.json)

# Optional cross-PR check: point to a reviewed CineScript Gem checkout/snapshot.
set(WANTED_CINESCRIPT_SOURCE "" CACHE PATH "CineScript Gem source for combined narrative integration tests")
if(WANTED_CINESCRIPT_SOURCE)
    add_subdirectory(${WANTED_CINESCRIPT_SOURCE}/Standalone ${CMAKE_CURRENT_BINARY_DIR}/cinescript)
    add_executable(WantedNarrativeCinematicTests NarrativeCinematicTests.cpp)
    target_link_libraries(WantedNarrativeCinematicTests PRIVATE WantedNarrative CineScriptCore)
    add_test(NAME Wanted.NarrativeCinematic COMMAND WantedNarrativeCinematicTests
        ${CMAKE_CURRENT_SOURCE_DIR}/../Assets/Narrative/unclaimed_post.narrative.json
        ${WANTED_CINESCRIPT_SOURCE}/../../Projects/Wanted/Assets/CineScript/mercy_delivery.cinescript.json
        ${WANTED_CINESCRIPT_SOURCE}/../../Projects/Wanted/Assets/ScriptMotion/courier.skeleton.json
        ${WANTED_CINESCRIPT_SOURCE}/../../Projects/Wanted/Assets/ScriptMotion/courier_wave.scriptmotion.json)
endif()
