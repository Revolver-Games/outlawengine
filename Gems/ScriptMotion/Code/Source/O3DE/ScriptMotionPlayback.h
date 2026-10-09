/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <ScriptMotion/O3DE/ScriptMotionConfiguration.h>

namespace EMotionFX
{
    class ActorInstance;
    class Motion;
    class MotionInstance;
}

namespace Wanted::ScriptMotion::O3DE
{
    // Shared by the game component and editor preview. All entry points are main-thread only.
    class ScriptMotionPlayback
    {
    public:
        ScriptMotionPlayback() = default;
        ~ScriptMotionPlayback();
        ScriptMotionPlayback(const ScriptMotionPlayback&) = delete;
        ScriptMotionPlayback& operator=(const ScriptMotionPlayback&) = delete;

        void SetActor(EMotionFX::ActorInstance* actor);
        bool Play(const ScriptMotionConfiguration& configuration);
        void Stop();
        bool SetPlaybackSpeed(float speed);
        bool Seek(float timeSeconds);
        float GetDuration() const;
        const AZStd::string& GetLastError() const { return m_error; }

    private:
        bool Fail(const char* error);
        EMotionFX::MotionInstance* GetLiveInstance() const;

        EMotionFX::ActorInstance* m_actor = nullptr; // Actor notification bus guards lifetime.
        EMotionFX::Motion* m_motion = nullptr;
        EMotionFX::MotionInstance* m_instance = nullptr;
        AZStd::string m_error;
    };
}
