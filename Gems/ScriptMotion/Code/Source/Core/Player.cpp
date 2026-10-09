/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <ScriptMotion/Player.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace Wanted::ScriptMotion
{
    namespace
    {
        bool InRange(double value, double maximum)
        {
            return std::isfinite(value) && value >= 0.0 && value <= maximum;
        }
    }

    std::string Player::Load(Skeleton skeleton, Clip clip, bool loop)
    {
        if (auto error = ValidateClip(clip, skeleton); !error.empty()) { return error; }
        m_skeleton = std::move(skeleton);
        m_clip = std::move(clip);
        m_layers.clear();
        m_fadeSource = {};
        m_root.reset();
        m_time = m_fadeDuration = m_fadeElapsed = 0.0;
        m_speed = 1.0;
        m_loop = loop;
        m_paused = m_completionSent = false;
        m_loaded = true;
        return {};
    }

    std::string Player::TransitionTo(Clip clip, double fadeSeconds, bool loop)
    {
        if (!m_loaded) { return "Load a skeleton and clip before transitioning"; }
        if (!InRange(fadeSeconds, 60.0)) { return "fadeSeconds must be finite in [0,60]"; }
        if (auto error = ValidateClip(clip, m_skeleton); !error.empty()) { return error; }
        auto source = SampleBase(m_time, m_fadeElapsed);
        if (!source) { return source.error; }
        m_fadeSource = std::move(source.value);
        m_clip = std::move(clip);
        m_loop = loop;
        m_time = m_fadeElapsed = 0.0;
        m_fadeDuration = fadeSeconds;
        m_completionSent = false;
        return {};
    }

    std::string Player::SetLayers(std::vector<Layer> layers)
    {
        if (!m_loaded) { return "Load a session before setting layers"; }
        if (layers.size() > 4) { return "At most four overlay layers are supported"; }
        Pose bind;
        for (const auto& bone : m_skeleton.bones) { bind.localTransforms.push_back(bone.bindPose); }
        for (const auto& layer : layers)
        {
            if (auto error = ValidateClip(layer.clip, m_skeleton); !error.empty()) { return error; }
            const auto check = layer.additive
                ? ApplyAdditiveLayer(bind, bind, layer.reference, layer.weight, layer.mask)
                : BlendPoses(bind, bind, layer.weight, layer.mask);
            if (!check) { return check.error; }
        }
        m_layers = std::move(layers);
        return {};
    }

    std::string Player::SetSpeed(double speed)
    {
        if (!InRange(speed, 16.0)) { return "speed must be finite in [0,16]"; }
        m_speed = speed;
        return {};
    }

    std::string Player::SetRootMotion(std::optional<std::size_t> root)
    {
        if (!m_loaded) { return "Load a session before setting root motion"; }
        if (root && (*root >= m_skeleton.bones.size() || m_skeleton.bones[*root].parent != -1))
        {
            return "Root motion requires a skeleton root bone";
        }
        m_root = root;
        return {};
    }

    void Player::Stop()
    {
        m_time = m_fadeDuration = m_fadeElapsed = 0.0;
        m_fadeSource = {};
        m_completionSent = false;
        m_paused = true;
    }

    Result<Pose> Player::SampleBase(double time, double fadeElapsed) const
    {
        auto pose = EvaluatePose(m_clip, m_skeleton, time, {1.0, m_loop});
        if (!pose || m_fadeDuration <= 0.0 || fadeElapsed >= m_fadeDuration) { return pose; }
        const double alpha = fadeElapsed / m_fadeDuration;
        return BlendPoses(m_fadeSource, pose.value, alpha * alpha * (3.0 - 2.0 * alpha));
    }

    Result<PlayerFrame> Player::SampleAt(double time, double fadeElapsed) const
    {
        if (!m_loaded) { return {{}, "No animation loaded"}; }
        auto pose = SampleBase(time, fadeElapsed);
        if (!pose) { return {{}, pose.error}; }
        for (const auto& layer : m_layers)
        {
            auto overlay = EvaluatePose(layer.clip, m_skeleton, time, {1.0, layer.loop});
            if (!overlay) { return {{}, overlay.error}; }
            pose = layer.additive
                ? ApplyAdditiveLayer(pose.value, overlay.value, layer.reference, layer.weight, layer.mask)
                : BlendPoses(pose.value, overlay.value, layer.weight, layer.mask);
            if (!pose) { return {{}, pose.error}; }
        }
        if (m_root)
        {
            pose.value.localTransforms[*m_root].translation = m_skeleton.bones[*m_root].bindPose.translation;
        }
        PlayerFrame frame;
        frame.pose = std::move(pose.value);
        return {std::move(frame), {}};
    }

    Result<Vec3> Player::RootPosition(double time) const
    {
        if (!m_root) { return {}; }
        auto sample = EvaluatePose(m_clip, m_skeleton, time, {1.0, m_loop});
        if (!sample) { return {{}, sample.error}; }
        Vec3 position = sample.value.localTransforms[*m_root].translation;
        if (m_loop)
        {
            auto start = EvaluatePose(m_clip, m_skeleton, 0.0, {1.0, false});
            auto end = EvaluatePose(m_clip, m_skeleton, m_clip.duration, {1.0, false});
            if (!start) { return {{}, start.error}; }
            if (!end) { return {{}, end.error}; }
            const auto a = start.value.localTransforms[*m_root].translation;
            const auto b = end.value.localTransforms[*m_root].translation;
            const double cycles = std::floor(time / m_clip.duration);
            position.x += cycles * (b.x - a.x);
            position.y += cycles * (b.y - a.y);
            position.z += cycles * (b.z - a.z);
        }
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            return {{}, "Root displacement overflow"};
        }
        return {position, {}};
    }

    Result<PlayerFrame> Player::Sample() const { return SampleAt(m_time, m_fadeElapsed); }

    Result<PlayerFrame> Player::Seek(double sourceSeconds)
    {
        if (!InRange(sourceSeconds, 86400.0)) { return {{}, "Seek time must be finite in [0,86400]"}; }
        const double time = m_loop ? sourceSeconds : std::min(sourceSeconds, m_clip.duration);
        auto frame = SampleAt(time, m_fadeDuration);
        if (!frame) { return frame; }
        m_time = time;
        m_fadeDuration = m_fadeElapsed = 0.0;
        m_fadeSource = {};
        m_completionSent = !m_loop && m_time >= m_clip.duration;
        return frame;
    }

    Result<PlayerFrame> Player::Advance(double deltaSeconds)
    {
        if (!InRange(deltaSeconds, 60.0)) { return {{}, "Frame delta must be finite in [0,60]"}; }
        if (m_paused || m_speed == 0.0 || deltaSeconds == 0.0) { return Sample(); }
        double next = m_time + deltaSeconds * m_speed;
        if (!m_loop) { next = std::min(next, m_clip.duration); }
        if (next > 86400.0) { return {{}, "Playback exceeds 24 hours; restart the session"}; }
        const double fade = std::min(m_fadeDuration, m_fadeElapsed + deltaSeconds);
        auto frame = SampleAt(next, fade);
        if (!frame) { return frame; }
        auto events = CollectEvents(m_clip, m_time, next, {1.0, m_loop}, 1024);
        if (!events) { return {{}, events.error}; }
        auto before = RootPosition(m_time);
        auto after = RootPosition(next);
        if (!before) { return {{}, before.error}; }
        if (!after) { return {{}, after.error}; }
        frame.value.events = std::move(events.value);
        frame.value.rootTranslationDelta = {after.value.x - before.value.x,
            after.value.y - before.value.y, after.value.z - before.value.z};
        frame.value.completed = !m_loop && next >= m_clip.duration && !m_completionSent;
        m_completionSent = m_completionSent || frame.value.completed;
        m_time = next;
        m_fadeElapsed = fade;
        return frame;
    }
}
