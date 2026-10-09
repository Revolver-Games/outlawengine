/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Wanted/WantedTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/string/string.h>

namespace Wanted
{
    class WantedRequests
    {
    public:
        AZ_RTTI(WantedRequests, WantedRequestsTypeId);
        virtual ~WantedRequests() = default;
        // Single-player prototype entry points. These are not network RPCs.
        virtual bool StartFirstErrand() = 0;
        virtual void ResetFirstErrand() = 0;
        virtual bool InteractWithMissionTarget(const AZStd::string& target) = 0;
        virtual AZ::u32 GetFirstErrandStage() const = 0;
        virtual AZStd::string GetFirstErrandObjective() const = 0;
    };

    class WantedBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using WantedRequestBus = AZ::EBus<WantedRequests, WantedBusTraits>;
    using WantedInterface = AZ::Interface<WantedRequests>;

    class WantedNotifications
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~WantedNotifications() = default;
        virtual void OnFirstErrandObjectiveChanged(const AZStd::string& objective) = 0;
        virtual void OnFirstErrandCompleted() = 0;
    };

    using WantedNotificationBus = AZ::EBus<WantedNotifications>;

} // namespace Wanted
