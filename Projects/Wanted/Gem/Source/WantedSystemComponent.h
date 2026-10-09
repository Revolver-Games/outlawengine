/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include <Wanted/WantedBus.h>
#include <Wanted/FirstErrand.h>

namespace Wanted
{
    class WantedSystemComponent
        : public AZ::Component
        , protected WantedRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(WantedSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        WantedSystemComponent();
        ~WantedSystemComponent();

        bool StartFirstErrand() override;
        void ResetFirstErrand() override;
        bool InteractWithMissionTarget(const AZStd::string& target) override;
        AZ::u32 GetFirstErrandStage() const override;
        AZStd::string GetFirstErrandObjective() const override;

    protected:
        ////////////////////////////////////////////////////////////////////////
        // WantedRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        void BroadcastObjective();
        FirstErrand m_firstErrand;
    };
}
