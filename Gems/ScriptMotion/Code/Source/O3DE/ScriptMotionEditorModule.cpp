/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <AzCore/Module/Module.h>
#include <ScriptMotion/O3DE/ScriptMotionComponent.h>
#include "ScriptMotionEditorComponent.h"

namespace Wanted::ScriptMotion::O3DE
{
    class ScriptMotionEditorModule final : public AZ::Module
    {
    public:
        AZ_RTTI(ScriptMotionEditorModule, "{E222C4B6-2764-4F78-BD70-B8E8F15F6E2F}", AZ::Module);
        AZ_CLASS_ALLOCATOR(ScriptMotionEditorModule, AZ::SystemAllocator);
        ScriptMotionEditorModule()
        {
            m_descriptors.push_back(ScriptMotionComponent::CreateDescriptor());
            m_descriptors.push_back(ScriptMotionEditorComponent::CreateDescriptor());
        }
    };
}
AZ_DECLARE_MODULE_CLASS(Gem_ScriptMotion, Wanted::ScriptMotion::O3DE::ScriptMotionEditorModule)
