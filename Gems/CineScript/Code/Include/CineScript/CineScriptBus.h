/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#pragma once
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/string/string.h>

namespace Wanted::CineScript
{
    class Requests : public AZ::ComponentBus
    {
    public:
        virtual bool Play() = 0;
        virtual void Cancel() = 0;
        virtual bool Seek(float seconds) = 0;
        virtual bool Skip() = 0;
        virtual void SetPaused(bool paused) = 0;
        virtual bool IsPlaying() const = 0;
        virtual AZStd::string GetLastError() const = 0;
        virtual AZStd::string SaveCheckpoint() const = 0;
        virtual bool RestoreCheckpoint(const AZStd::string& json) = 0;
    };
    using RequestBus = AZ::EBus<Requests>;

    class Notifications : public AZ::ComponentBus
    {
    public:
        virtual void OnControlLockChanged(bool) {}
        virtual void OnSubtitle(const AZStd::string&, const AZStd::string&) {}
        virtual void OnCue(const AZStd::string&, const AZStd::string&) {}
        virtual void OnCompleted(bool /* skipped */) {}
        virtual void OnError(const AZStd::string&) {}
    };
    using NotificationBus = AZ::EBus<Notifications>;
}
