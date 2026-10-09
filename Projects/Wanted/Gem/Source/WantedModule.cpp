/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

#include "WantedSystemComponent.h"

#include <Wanted/WantedTypeIds.h>

namespace Wanted
{
    class WantedModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(WantedModule, WantedModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(WantedModule, AZ::SystemAllocator);

        WantedModule()
            : AZ::Module()
        {
            // Register the WANTED mission system component.
            m_descriptors.insert(m_descriptors.end(), {
                WantedSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<WantedSystemComponent>(),
            };
        }
    };
}// namespace Wanted

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Wanted::WantedModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Wanted, Wanted::WantedModule)
#endif
