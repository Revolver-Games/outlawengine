/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#pragma once
#include <ScriptMotion/ScriptMotion.h>
#include <optional>

namespace Wanted::ScriptMotion
{
    struct Layer
    {
        Clip clip;
        double weight = 1.0;
        std::vector<double> mask;
        bool additive = false;
        Pose reference; // Required for additive layers; usually the bind pose.
        bool loop = true;
    };

    struct PlayerFrame
    {
        Pose pose;
        std::vector<EventOccurrence> events; // Indices in GetClip(); base clip only.
        Vec3 rootTranslationDelta; // Parent-space displacement; never applied to an entity here.
        bool completed = false; // Edge, emitted once by Advance, never by Seek.
    };

    // Owning, deterministic playback for tools, headless simulation and runtime consumers.
    // Failed operations preserve the current session. Not thread-safe. No callbacks or I/O.
    class Player
    {
    public:
        [[nodiscard]] std::string Load(Skeleton skeleton, Clip clip, bool loop = true);
        [[nodiscard]] std::string TransitionTo(Clip clip, double fadeSeconds, bool loop = true);
        [[nodiscard]] std::string SetLayers(std::vector<Layer> layers);
        [[nodiscard]] std::string SetSpeed(double speed);
        [[nodiscard]] std::string SetRootMotion(std::optional<std::size_t> root);
        void SetPaused(bool paused) { m_paused = paused; }
        void Stop(); // Rewind, clear transition, pause; retain loaded data/layers.
        [[nodiscard]] Result<PlayerFrame> Sample() const;
        [[nodiscard]] Result<PlayerFrame> Seek(double sourceSeconds); // Silent; cancels crossfade.
        [[nodiscard]] Result<PlayerFrame> Advance(double deltaSeconds);
        [[nodiscard]] double GetTime() const { return m_time; }
        [[nodiscard]] double GetSpeed() const { return m_speed; }
        [[nodiscard]] bool IsPaused() const { return m_paused; }
        [[nodiscard]] const Clip& GetClip() const { return m_clip; }
    private:
        [[nodiscard]] Result<Pose> SampleBase(double time, double fadeElapsed) const;
        [[nodiscard]] Result<PlayerFrame> SampleAt(double time, double fadeElapsed) const;
        [[nodiscard]] Result<Vec3> RootPosition(double time) const;
        Skeleton m_skeleton;
        Clip m_clip;
        std::vector<Layer> m_layers;
        Pose m_fadeSource;
        std::optional<std::size_t> m_root;
        double m_time = 0.0;
        double m_speed = 1.0;
        double m_fadeDuration = 0.0;
        double m_fadeElapsed = 0.0;
        bool m_loop = true;
        bool m_paused = false;
        bool m_loaded = false;
        bool m_completionSent = false;
    };
}
