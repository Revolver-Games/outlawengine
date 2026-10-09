# Copyright (c) 2026 Revolver Games contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
ly_add_target(NAME CineScript.Core STATIC NAMESPACE Gem
    FILES_CMAKE cinescript_core_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ScriptMotion/Code/Source/Core
    BUILD_DEPENDENCIES PUBLIC Gem::ScriptMotion.Core)
target_compile_features(CineScript.Core PUBLIC cxx_std_20)
ly_add_target(NAME CineScript.Static STATIC NAMESPACE Gem
    FILES_CMAKE cinescript_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE Source
    BUILD_DEPENDENCIES PUBLIC AZ::AzCore AZ::AzFramework Gem::CineScript.Core Gem::ScriptMotion.Static Gem::DebugDraw.API)
target_compile_features(CineScript.Static PUBLIC cxx_std_20)
ly_add_target(NAME CineScript ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE} NAMESPACE Gem
    FILES_CMAKE cinescript_module_files.cmake
    INCLUDE_DIRECTORIES PRIVATE Source
    BUILD_DEPENDENCIES PRIVATE Gem::CineScript.Static)
target_compile_features(CineScript PRIVATE cxx_std_20)
foreach(variant IN ITEMS Clients Unified)
    ly_create_alias(NAME CineScript.${variant} NAMESPACE Gem TARGETS CineScript)
endforeach()
if(PAL_TRAIT_BUILD_HOST_TOOLS)
    # GenericComponentWrapper exposes the reflected runtime configuration in the editor.
    ly_create_alias(NAME CineScript.Tools NAMESPACE Gem TARGETS CineScript)
    ly_create_alias(NAME CineScript.Builders NAMESPACE Gem TARGETS CineScript)
    o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME CineScript VARIANTS Tools Builders)
endif()
o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME CineScript VARIANTS Clients Unified)
