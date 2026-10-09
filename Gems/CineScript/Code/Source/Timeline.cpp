/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <CineScript/Timeline.h>
#define JSON_NOEXCEPTION 1
#include <ThirdParty/nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Wanted::CineScript
{
    namespace
    {
        using Json = nlohmann::json;
        bool Name(const std::string& s)
        {
            return !s.empty() && s.size() <= 128 && std::all_of(s.begin(), s.end(), [](unsigned char c)
            { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
        }
        bool Range(double n, double maximum) { return std::isfinite(n) && n >= 0.0 && n <= maximum; }
        bool Fields(const Json& j, std::initializer_list<std::string_view> names)
        {
            if (!j.is_object()) { return false; }
            for (auto it = j.begin(); it != j.end(); ++it)
            {
                if (std::find(names.begin(), names.end(), it.key()) == names.end()) { return false; }
            }
            return true;
        }
        bool String(const Json& j, const char* name, std::string& out, bool optional = false)
        {
            auto it = j.find(name);
            if (it == j.end()) { return optional; }
            if (!it->is_string()) { return false; }
            out = it->get<std::string>();
            return true;
        }
        bool Number(const Json& j, const char* name, double& out, bool optional = false)
        {
            auto it = j.find(name);
            if (it == j.end()) { return optional; }
            if (!it->is_number()) { return false; }
            out = it->get<double>();
            return std::isfinite(out);
        }
        Result<Json> Document(std::string_view text)
        {
            if (text.empty() || text.size() > 1024 * 1024) { return {{}, "Document must be 1 byte to 1 MiB"}; }
            // Bound nesting before allocating a DOM. Brackets inside strings are ignored.
            std::size_t depth = 0;
            bool quoted = false, escaped = false;
            for (char c : text)
            {
                if (quoted)
                {
                    if (escaped) { escaped = false; }
                    else if (c == '\\') { escaped = true; }
                    else if (c == '"') { quoted = false; }
                }
                else if (c == '"') { quoted = true; }
                else if (c == '{' || c == '[') { if (++depth > 16) { return {{}, "JSON nesting exceeds 16"}; } }
                else if (c == '}' || c == ']') { if (!depth) { return {{}, "Invalid delimiter"}; } --depth; }
            }
            bool duplicate = false;
            std::vector<std::unordered_set<std::string>> keys;
            auto callback = [&](int, Json::parse_event_t event, Json& value)
            {
                if (event == Json::parse_event_t::object_start) { keys.emplace_back(); }
                else if (event == Json::parse_event_t::key && !keys.empty())
                { if (!keys.back().insert(value.get<std::string>()).second) { duplicate = true; } }
                else if (event == Json::parse_event_t::object_end && !keys.empty()) { keys.pop_back(); }
                return true;
            };
            auto result = Json::parse(text.begin(), text.end(), callback, false);
            if (result.is_discarded() || !result.is_object() || duplicate) { return {{}, "Invalid JSON or duplicate object key"}; }
            return {std::move(result), {}};
        }
        SM::Skeleton TrackRig() { return {"cine", {{"world", -1, {}}}}; }
        SM::Clip TrackClip(const Track& track, double duration)
        { return {"placement", "cine", duration, {{0, track.keys}}, {}}; }
        bool IsCue(EventKind kind) { return kind == EventKind::Signal || kind == EventKind::Audio || kind == EventKind::Music; }
    }

    std::string ValidateScene(const Scene& scene)
    {
        if (!Name(scene.id) || scene.revision == 0 || !Range(scene.duration, 600) || scene.duration < 0.001)
        { return "Invalid scene id, revision or duration (0.001..600 seconds)"; }
        if (scene.targets.empty() || scene.targets.size() > 64 || scene.tracks.size() > 64 || scene.events.size() > 1024)
        { return "Scene exceeds target/track/event limits (64/64/1024)"; }
        std::unordered_set<std::string> names, tracked;
        for (const auto& t : scene.targets)
        {
            if (!Name(t.id) || !names.insert(t.id).second || (t.kind != TargetKind::Actor && t.kind != TargetKind::Camera))
            { return "Invalid or duplicate target"; }
        }
        std::size_t keys = 0;
        for (const auto& t : scene.tracks)
        {
            keys += t.keys.size();
            if (!names.contains(t.target) || !tracked.insert(t.target).second || t.keys.empty() || t.keys.front().time != 0 || keys > 4096)
            { return "Invalid track target/start or key budget (4096)"; }
            if (auto error = SM::ValidateClip(TrackClip(t, scene.duration), TrackRig()); !error.empty()) { return error; }
        }
        double last = -1;
        bool initialCamera = false;
        for (std::size_t i = 0; i < scene.events.size(); ++i)
        {
            const auto& e = scene.events[i];
            if (!Range(e.time, scene.duration) || e.time < last || !Range(e.duration, scene.duration - e.time)
                || (!e.condition.empty() && !Name(e.condition))) { return "Invalid event timing or condition"; }
            last = e.time;
            const auto target = std::find_if(scene.targets.begin(), scene.targets.end(), [&](const auto& t) { return t.id == e.target; });
            if (e.kind == EventKind::Camera)
            {
                if (target == scene.targets.end() || target->kind != TargetKind::Camera || !e.value.empty() || e.duration != 0)
                { return "Camera cut requires a camera target and no value/duration"; }
                initialCamera |= e.time == 0 && e.condition.empty();
            }
            else if (e.kind == EventKind::Dialogue || e.kind == EventKind::Animation)
            {
                if (target == scene.targets.end() || target->kind != TargetKind::Actor || e.duration <= 0 || e.value.empty()
                    || e.value.size() > 2048 || e.value.find('\0') != std::string::npos)
                { return "Dialogue/animation requires actor, value and positive duration"; }
                if (e.kind == EventKind::Animation && !Name(e.value)) { return "Animation value must be a bound clip alias"; }
                for (std::size_t j = 0; j < i; ++j)
                {
                    const auto& before = scene.events[j];
                    if (before.kind == e.kind && (e.kind == EventKind::Dialogue || before.target == e.target)
                        && before.time + before.duration > e.time)
                    { return "Overlapping dialogue or animation on the same actor is unsupported"; }
                }
            }
            else if (IsCue(e.kind))
            {
                if (!Name(e.value) || e.duration != 0 || !e.target.empty()) { return "Cue requires an alias and no target/duration"; }
            }
            else { return "Unknown event kind"; }
        }
        if (!initialCamera) { return "An unconditional camera cut at time zero is required"; }
        return {};
    }

    Result<Scene> ParseScene(std::string_view text)
    {
        auto document = Document(text);
        if (!document) { return {{}, document.error}; }
        const auto& j = document.value;
        Scene s;
        double revision = 0, version = 0;
        if (!Fields(j, {"formatVersion", "id", "revision", "duration", "targets", "tracks", "events"})
            || !Number(j, "formatVersion", version) || version != 1 || !String(j, "id", s.id)
            || !Number(j, "revision", revision) || revision < 1 || revision > 1000000000 || std::floor(revision) != revision
            || !Number(j, "duration", s.duration)) { return {{}, "Invalid scene header"}; }
        s.revision = static_cast<std::uint32_t>(revision);
        for (const char* field : {"targets", "tracks", "events"})
        {
            const auto it = j.find(field);
            if (it == j.end() || !it->is_array() || it->size() > (std::string_view(field) == "events" ? 1024u : 64u))
            { return {{}, std::string("Invalid or oversized array: ") + field}; }
        }
        for (const auto& item : j["targets"])
        {
            Target t; std::string kind;
            if (!Fields(item, {"id", "kind"}) || !String(item, "id", t.id) || !String(item, "kind", kind)
                || (kind != "actor" && kind != "camera")) { return {{}, "Invalid target"}; }
            t.kind = kind == "actor" ? TargetKind::Actor : TargetKind::Camera;
            s.targets.push_back(std::move(t));
        }
        for (const auto& item : j["tracks"])
        {
            Track t;
            if (!Fields(item, {"target", "keys"}) || !String(item, "target", t.target) || !item.contains("keys"))
            { return {{}, "Invalid track"}; }
            // Reuse the existing strict transform/keyframe parser instead of inventing another quaternion format.
            Json clip = {{"formatVersion", 1}, {"name", "placement"}, {"skeleton", "cine"}, {"duration", s.duration},
                {"tracks", Json::array({{{"bone", "world"}, {"keys", item["keys"]}}})}};
            auto parsed = SM::ParseClip(clip.dump(), TrackRig());
            if (!parsed) { return {{}, "Track " + t.target + ": " + parsed.error}; }
            t.keys = std::move(parsed.value.tracks.front().keys);
            s.tracks.push_back(std::move(t));
        }
        for (const auto& item : j["events"])
        {
            Event e; std::string type;
            if (!Fields(item, {"time", "duration", "type", "target", "value", "if"})
                || !Number(item, "time", e.time) || !Number(item, "duration", e.duration, true)
                || !String(item, "type", type) || !String(item, "target", e.target, true)
                || !String(item, "value", e.value, true) || !String(item, "if", e.condition, true))
            { return {{}, "Invalid event fields"}; }
            if (type == "camera") { e.kind = EventKind::Camera; }
            else if (type == "dialogue") { e.kind = EventKind::Dialogue; }
            else if (type == "animation") { e.kind = EventKind::Animation; }
            else if (type == "signal") { e.kind = EventKind::Signal; }
            else if (type == "audio") { e.kind = EventKind::Audio; }
            else if (type == "music") { e.kind = EventKind::Music; }
            else { return {{}, "Unknown event type: " + type}; }
            s.events.push_back(std::move(e));
        }
        if (auto error = ValidateScene(s); !error.empty()) { return {{}, error}; }
        return {std::move(s), {}};
    }

    bool Timeline::Enabled(const Event& e) const
    { return e.condition.empty() || std::find(m_flags.begin(), m_flags.end(), e.condition) != m_flags.end(); }

    std::string Timeline::Load(Scene scene, std::vector<std::string> flags)
    {
        if (auto error = ValidateScene(scene); !error.empty()) { return error; }
        if (flags.size() > 128 || !std::all_of(flags.begin(), flags.end(), Name)) { return "Invalid condition flags"; }
        std::sort(flags.begin(), flags.end());
        if (std::adjacent_find(flags.begin(), flags.end()) != flags.end()) { return "Duplicate condition flags"; }
        m_scene = std::move(scene); m_flags = std::move(flags); m_loaded = true;
        Stop();
        return {};
    }

    Result<Frame> Timeline::At(double time) const
    {
        if (!m_loaded) { return {{}, "No cutscene loaded"}; }
        Frame f; f.time = time; f.gameplayLocked = m_playing;
        for (const auto& t : m_scene.tracks)
        {
            auto pose = SM::EvaluatePose(TrackClip(t, m_scene.duration), TrackRig(), time, {1, false});
            if (!pose) { return {{}, pose.error}; }
            f.placements.push_back({t.target, pose.value.localTransforms.front()});
        }
        for (const auto& e : m_scene.events)
        {
            if (e.time > time || !Enabled(e)) { continue; }
            if (e.kind == EventKind::Camera) { f.camera = e.target; }
            else if (e.kind == EventKind::Dialogue && time < e.time + e.duration)
            { f.speaker = e.target; f.subtitle = e.value; }
            else if (e.kind == EventKind::Animation && time < e.time + e.duration)
            { f.animations.push_back({e.target, e.value, time - e.time}); }
        }
        return {std::move(f), {}};
    }
    Result<Frame> Timeline::Sample() const { return At(m_time); }
    Result<Frame> Timeline::Start()
    {
        if (m_playing) { return {{}, "Cutscene is already playing"}; }
        auto frame = At(0);
        if (!frame) { return frame; }
        for (const auto& e : m_scene.events) { if (e.time == 0 && Enabled(e) && IsCue(e.kind)) { frame.value.cues.push_back(e); } }
        m_time = 0; m_playing = true; m_paused = m_finished = false;
        frame.value.started = frame.value.gameplayLocked = true;
        return frame;
    }
    Result<Frame> Timeline::Advance(double seconds)
    {
        if (!Range(seconds, 60)) { return {{}, "Frame delta must be finite in [0,60]"}; }
        if (!m_playing || m_paused || seconds == 0) { return Sample(); }
        const double next = std::min(m_time + seconds, m_scene.duration);
        auto frame = At(next);
        if (!frame) { return frame; }
        for (const auto& e : m_scene.events)
        { if (e.time > m_time && e.time <= next && Enabled(e) && IsCue(e.kind)) { frame.value.cues.push_back(e); } }
        m_time = next;
        if (next == m_scene.duration)
        { m_playing = false; m_finished = true; frame.value.completed = true; frame.value.gameplayLocked = false; }
        return frame;
    }
    Result<Frame> Timeline::Seek(double seconds)
    {
        if (!Range(seconds, m_scene.duration)) { return {{}, "Seek outside scene duration"}; }
        auto frame = At(seconds);
        if (!frame) { return frame; }
        m_time = seconds;
        m_finished = seconds == m_scene.duration;
        if (m_finished) { m_playing = false; frame.value.gameplayLocked = false; }
        return frame;
    }
    Result<Frame> Timeline::Skip()
    {
        if (!m_playing) { return {{}, "No playing cutscene to skip"}; }
        auto frame = At(m_scene.duration);
        if (!frame) { return frame; }
        m_time = m_scene.duration; m_playing = false; m_finished = true;
        frame.value.completed = true; frame.value.gameplayLocked = false;
        return frame;
    }
    void Timeline::Stop() { m_time = 0; m_playing = m_paused = m_finished = false; }

    std::string Timeline::SaveCheckpoint() const
    {
        if (!m_loaded) { return {}; }
        return Json{{"formatVersion", 1}, {"scene", m_scene.id}, {"revision", m_scene.revision}, {"time", m_time},
            {"playing", m_playing}, {"paused", m_paused}, {"finished", m_finished}, {"flags", m_flags}}.dump();
    }
    std::string Timeline::RestoreCheckpoint(std::string_view text)
    {
        if (!m_loaded) { return "Load the matching scene before restoring"; }
        auto doc = Document(text);
        if (!doc) { return doc.error; }
        const auto& j = doc.value;
        if (!Fields(j, {"formatVersion", "scene", "revision", "time", "playing", "paused", "finished", "flags"}))
        { return "Unknown checkpoint field"; }
        std::string id; double revision = 0, time = 0, version = 0;
        if (!String(j, "scene", id) || id != m_scene.id || !Number(j, "revision", revision) || revision != m_scene.revision
            || !Number(j, "formatVersion", version) || version != 1 || !Number(j, "time", time) || !Range(time, m_scene.duration))
        { return "Checkpoint scene/revision/time mismatch"; }
        for (const char* key : {"playing", "paused", "finished"})
        { if (!j.contains(key) || !j[key].is_boolean()) { return "Checkpoint state must be boolean"; } }
        const bool playing = j["playing"].get<bool>(), finished = j["finished"].get<bool>();
        if (finished != (time == m_scene.duration) || (playing && finished)) { return "Inconsistent checkpoint completion state"; }
        if (!j.contains("flags") || !j["flags"].is_array() || j["flags"].size() > 128) { return "Invalid checkpoint flags"; }
        std::vector<std::string> flags;
        for (const auto& flag : j["flags"])
        { if (!flag.is_string()) { return "Invalid checkpoint flag"; } flags.push_back(flag.get<std::string>()); }
        auto copy = *this;
        if (auto error = copy.Load(m_scene, std::move(flags)); !error.empty()) { return error; }
        copy.m_time = time; copy.m_playing = playing; copy.m_paused = j["paused"].get<bool>(); copy.m_finished = finished;
        *this = std::move(copy);
        return {};
    }
}
