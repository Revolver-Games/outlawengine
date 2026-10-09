/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#pragma once
#include <ScriptMotion/ScriptMotion.h>
#include <cstdint>

namespace Wanted::CineScript
{
    namespace SM = ScriptMotion;
    template<class T> using Result = SM::Result<T>;
    enum class TargetKind { Actor, Camera };
    struct Target { std::string id; TargetKind kind = TargetKind::Actor; };
    struct Track { std::string target; std::vector<SM::Keyframe> keys; };
    enum class EventKind { Camera, Dialogue, Animation, Signal, Audio, Music };
    struct Event
    {
        double time = 0.0;
        double duration = 0.0;
        EventKind kind = EventKind::Signal;
        std::string target;
        std::string value;
        std::string condition; // Required true flag; empty means unconditional.
    };
    struct Scene
    {
        std::string id;
        std::uint32_t revision = 1;
        double duration = 0.0;
        std::vector<Target> targets;
        std::vector<Track> tracks;
        std::vector<Event> events;
    };
    struct Placement { std::string target; SM::LocalTransform transform; };
    struct AnimationState { std::string target; std::string clip; double time = 0.0; };
    struct Frame
    {
        double time = 0.0;
        std::vector<Placement> placements;
        std::string camera;
        std::string speaker;
        std::string subtitle;
        std::vector<AnimationState> animations;
        std::vector<Event> cues; // Signal/audio/music crossings, in authored order.
        bool started = false;
        bool completed = false;
        bool gameplayLocked = false;
    };

    [[nodiscard]] Result<Scene> ParseScene(std::string_view json);
    [[nodiscard]] std::string ValidateScene(const Scene& scene);
    // World-space Z-up transforms. All tracks start at zero; first camera cut is at zero.
    // No script evaluation or I/O; identifiers refer to host-provided bindings.
    class Timeline
    {
    public:
        [[nodiscard]] std::string Load(Scene scene, std::vector<std::string> trueFlags = {});
        [[nodiscard]] Result<Frame> Start();
        [[nodiscard]] Result<Frame> Advance(double seconds);
        [[nodiscard]] Result<Frame> Seek(double seconds); // Silent; preserves playing/paused state.
        [[nodiscard]] Result<Frame> Skip(); // End once, suppress pending side-effect cues.
        [[nodiscard]] Result<Frame> Sample() const;
        void Pause(bool paused) { m_paused = paused; }
        void Stop(); // Stop and rewind silently; host releases camera/control ownership.
        [[nodiscard]] bool IsPlaying() const { return m_playing; }
        [[nodiscard]] bool IsPaused() const { return m_paused; }
        [[nodiscard]] const Scene& GetScene() const { return m_scene; }
        [[nodiscard]] std::string SaveCheckpoint() const;
        [[nodiscard]] std::string RestoreCheckpoint(std::string_view json); // Silent; no cue replay.
    private:
        [[nodiscard]] Result<Frame> At(double time) const;
        [[nodiscard]] bool Enabled(const Event& event) const;
        Scene m_scene;
        std::vector<std::string> m_flags;
        double m_time = 0.0;
        bool m_loaded = false;
        bool m_playing = false;
        bool m_paused = false;
        bool m_finished = false;
    };
}
