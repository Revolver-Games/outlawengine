/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <AzCore/Module/Module.h>
#include <ScriptMotion/O3DE/ScriptMotionComponent.h>

namespace Wanted::ScriptMotion::O3DE
{
    class ScriptMotionModule final : public AZ::Module
    {
    public:
        AZ_RTTI(ScriptMotionModule, "{C1B62572-258F-4A2D-B1A0-4306361E92BE}", AZ::Module);
        AZ_CLASS_ALLOCATOR(ScriptMotionModule, AZ::SystemAllocator);
        ScriptMotionModule()
        {
            m_descriptors.push_back(ScriptMotionComponent::CreateDescriptor());
        }
    };
}
AZ_DECLARE_MODULE_CLASS(Gem_ScriptMotion, Wanted::ScriptMotion::O3DE::ScriptMotionModule)
