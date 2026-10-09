/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <AzCore/Module/Module.h>
#include "CineScriptComponent.h"
namespace Wanted::CineScript
{
    class CineScriptModule final : public AZ::Module
    {
    public:
        AZ_RTTI(CineScriptModule, "{C97F265E-B79F-4C1D-8A18-E8F4694FB8BD}", AZ::Module);
        AZ_CLASS_ALLOCATOR(CineScriptModule, AZ::SystemAllocator);
        CineScriptModule() { m_descriptors.push_back(CineScriptComponent::CreateDescriptor()); }
    };
}
AZ_DECLARE_MODULE_CLASS(Gem_CineScript, Wanted::CineScript::CineScriptModule)
