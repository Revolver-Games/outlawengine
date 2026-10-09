/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/string/string.h>

namespace Wanted::ScriptMotion::O3DE
{
    class ScriptMotionRequests : public AZ::ComponentBus
    {
    public:
        // Commands run on the main thread, as do O3DE's built-in simple-motion controls.
        virtual bool Play() = 0;
        virtual void Stop() = 0;
        virtual bool SetPlaybackSpeed(float speed) = 0;
        virtual bool Seek(float timeSeconds) = 0;
        virtual float GetDuration() const = 0;
        virtual AZStd::string GetLastError() const = 0;
    };
    using ScriptMotionRequestBus = AZ::EBus<ScriptMotionRequests>;
}
