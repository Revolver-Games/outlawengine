# Copyright (c) 2026 Wanted Engine contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT

ly_add_target(
    NAME ScriptMotion.Core STATIC
    NAMESPACE Gem
    FILES_CMAKE scriptmotion_core_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE Source/Core
)
target_compile_features(ScriptMotion.Core PUBLIC cxx_std_20)

ly_add_target(
    NAME ScriptMotion.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE scriptmotion_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE Source
    BUILD_DEPENDENCIES PUBLIC AZ::AzCore AZ::AzFramework Gem::EMotionFXStaticLib Gem::ScriptMotion.Core
)
target_compile_features(ScriptMotion.Static PUBLIC cxx_std_20)

ly_add_target(
    NAME ScriptMotion ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE scriptmotion_module_files.cmake
    INCLUDE_DIRECTORIES PRIVATE Source
    BUILD_DEPENDENCIES PRIVATE Gem::ScriptMotion.Static
)
target_compile_features(ScriptMotion PRIVATE cxx_std_20)
ly_create_alias(NAME ScriptMotion.Clients NAMESPACE Gem TARGETS ScriptMotion)
ly_create_alias(NAME ScriptMotion.Servers NAMESPACE Gem TARGETS ScriptMotion)
ly_create_alias(NAME ScriptMotion.Unified NAMESPACE Gem TARGETS ScriptMotion)
o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME ScriptMotion VARIANTS Clients Servers Unified)

if(PAL_TRAIT_BUILD_HOST_TOOLS)
    ly_add_target(
        NAME ScriptMotion.Editor GEM_MODULE
        NAMESPACE Gem
        FILES_CMAKE scriptmotion_editor_files.cmake
        INCLUDE_DIRECTORIES PRIVATE Source
        BUILD_DEPENDENCIES PRIVATE Gem::ScriptMotion.Static AZ::AzToolsFramework
    )
    target_compile_features(ScriptMotion.Editor PRIVATE cxx_std_20)
    ly_create_alias(NAME ScriptMotion.Tools NAMESPACE Gem TARGETS ScriptMotion.Editor)
    ly_create_alias(NAME ScriptMotion.Builders NAMESPACE Gem TARGETS ScriptMotion.Editor)
    o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME ScriptMotion VARIANTS Tools Builders)
endif()
