/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/ScriptMotion.h>

#define JSON_NOEXCEPTION 1
#include "ThirdParty/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Wanted::ScriptMotion
{
    namespace
    {
        using Json = nlohmann::json;
        constexpr double MinDuration = 1e-6;
        constexpr double MaxDuration = 86400.0;
        constexpr double MaxTimeline = 1e9;
        constexpr double MaxSpeed = 100.0;
        constexpr std::size_t MaxDepth = 32;

        template<class T>
        Result<T> Failure(std::string error)
        {
            return { {}, std::move(error) };
        }

        bool ValidName(const std::string& name)
        {
            return !name.empty() && name.size() <= 128 && name.find('\0') == std::string::npos;
        }

        bool Finite(Vec3 value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        double Norm(Quaternion value)
        {
            return std::hypot(std::hypot(value.x, value.y), std::hypot(value.z, value.w));
        }

        bool ValidRotation(Quaternion value)
        {
            const double norm = Norm(value);
            return std::isfinite(norm) && norm >= 1e-12;
        }

        Quaternion Normalize(Quaternion value)
        {
            const double norm = Norm(value);
            return { value.x / norm, value.y / norm, value.z / norm, value.w / norm };
        }

        Quaternion Multiply(Quaternion a, Quaternion b)
        {
            return {
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
            };
        }

        bool ValidTransform(const LocalTransform& value)
        {
            // Bounds keep conversion to engine float coordinates and interpolation finite.
            constexpr double MaxCoordinate = 1e9;
            return Finite(value.translation) && std::abs(value.translation.x) <= MaxCoordinate
                && std::abs(value.translation.y) <= MaxCoordinate && std::abs(value.translation.z) <= MaxCoordinate
                && ValidRotation(value.rotation);
        }

        std::string Fields(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view where)
        {
            if (!object.is_object())
            {
                return std::string(where) + " must be an object";
            }
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end())
                {
                    return std::string(where) + ": unknown field '" + it.key() + "'";
                }
            }
            return {};
        }

        Result<Json> ReadJson(std::string_view text)
        {
            if (text.empty() || text.size() > MaxJsonBytes)
            {
                return Failure<Json>("JSON must contain 1 to " + std::to_string(MaxJsonBytes) + " bytes");
            }
            // Preflight nesting before constructing the recursive DOM; braces inside strings do not count.
            std::size_t depth = 0;
            bool inString = false;
            bool escaped = false;
            for (const char c : text)
            {
                if (inString)
                {
                    if (escaped) { escaped = false; }
                    else if (c == '\\') { escaped = true; }
                    else if (c == '"') { inString = false; }
                }
                else if (c == '"') { inString = true; }
                else if (c == '{' || c == '[')
                {
                    if (++depth > MaxDepth) { return Failure<Json>("JSON nesting exceeds 32 levels"); }
                }
                else if (c == '}' || c == ']')
                {
                    if (depth == 0) { return Failure<Json>("Malformed JSON closing delimiter"); }
                    --depth;
                }
            }
            bool duplicate = false;
            std::vector<std::unordered_set<std::string>> objectKeys;
            auto callback = [&duplicate, &objectKeys](int, Json::parse_event_t event, Json& parsed)
            {
                if (event == Json::parse_event_t::object_start) { objectKeys.emplace_back(); }
                else if (event == Json::parse_event_t::key && !objectKeys.empty())
                {
                    if (!objectKeys.back().insert(parsed.get<std::string>()).second) { duplicate = true; }
                }
                else if (event == Json::parse_event_t::object_end && !objectKeys.empty()) { objectKeys.pop_back(); }
                return true;
            };
            Json document = Json::parse(text.begin(), text.end(), callback, false);
            if (document.is_discarded()) { return Failure<Json>("Malformed JSON or nonfinite numeric literal"); }
            if (duplicate) { return Failure<Json>("Duplicate JSON object key"); }
            if (!document.is_object()) { return Failure<Json>("Document root must be an object"); }
            return { std::move(document), {} };
        }

        bool Number(const Json& object, const char* field, double& output)
        {
            const auto it = object.find(field);
            if (it == object.end() || !it->is_number()) { return false; }
            output = it->get<double>();
            return std::isfinite(output);
        }

        bool Name(const Json& object, const char* field, std::string& output)
        {
            const auto it = object.find(field);
            if (it == object.end() || !it->is_string()) { return false; }
            output = it->get<std::string>();
            return ValidName(output);
        }

        bool Version(const Json& object)
        {
            const auto it = object.find("formatVersion");
            return it != object.end() && it->is_number_integer() && *it == 1;
        }

        bool NumberArray(const Json& array, double* output, std::size_t count)
        {
            if (!array.is_array() || array.size() != count) { return false; }
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!array[i].is_number()) { return false; }
                output[i] = array[i].get<double>();
                if (!std::isfinite(output[i])) { return false; }
            }
            return true;
        }

        std::string Transform(const Json& object, LocalTransform& transform)
        {
            if (const auto it = object.find("translation"); it != object.end())
            {
                double components[3]{};
                if (!NumberArray(*it, components, 3)) { return "translation must be three finite numbers"; }
                transform.translation = { components[0], components[1], components[2] };
            }
            if (const auto it = object.find("rotation"); it != object.end())
            {
                double components[4]{};
                if (!NumberArray(*it, components, 4)) { return "rotation must be four finite numbers in x,y,z,w order"; }
                transform.rotation = { components[0], components[1], components[2], components[3] };
            }
            if (!ValidTransform(transform)) { return "transform has invalid coordinates or a zero/nonfinite quaternion"; }
            transform.rotation = Normalize(transform.rotation);
            return {};
        }

        std::string Timing(const Clip& clip, double time, PlaybackOptions options)
        {
            if (!std::isfinite(clip.duration) || clip.duration < MinDuration || clip.duration > MaxDuration)
            {
                return "duration must be between 0.000001 and 86400 seconds";
            }
            if (!std::isfinite(time) || time < 0.0 || time > MaxTimeline)
            {
                return "timeline must be finite and between 0 and 1000000000 seconds";
            }
            if (!std::isfinite(options.speed) || options.speed < 0.0 || options.speed > MaxSpeed)
            {
                return "playback speed must be finite and between 0 and 100";
            }
            // MSVC long double has double precision. Keep cycle ordinals well below 2^53
            // so adding one boundary is meaningful on every supported compiler.
            if (options.loop && time * options.speed / clip.duration > 1e12)
            {
                return "Loop timeline exceeds the supported precision range (1000000000000 cycles)";
            }
            return {};
        }

        std::string ValidEvents(const Clip& clip)
        {
            if (clip.events.size() > MaxEvents) { return "Too many animation events"; }
            double last = -1.0;
            for (const AnimationEvent& event : clip.events)
            {
                if (!std::isfinite(event.time) || event.time < 0.0 || event.time > clip.duration || event.time < last)
                {
                    return "Events must be ordered nondecreasingly inside the clip duration";
                }
                if (!ValidName(event.name) || event.payloadJson.size() > MaxJsonBytes)
                {
                    return "Invalid event name or oversized event payload";
                }
                last = event.time;
            }
            return {};
        }

        LocalTransform Interpolate(const LocalTransform& a, const LocalTransform& b, double alpha)
        {
            return {
                { std::lerp(a.translation.x, b.translation.x, alpha), std::lerp(a.translation.y, b.translation.y, alpha),
                  std::lerp(a.translation.z, b.translation.z, alpha) },
                Slerp(a.rotation, b.rotation, alpha)
            };
        }

        std::string ValidatePose(const Pose& pose)
        {
            if (pose.localTransforms.empty() || pose.localTransforms.size() > MaxBones) { return "Invalid pose bone count"; }
            for (const LocalTransform& transform : pose.localTransforms)
            {
                if (!ValidTransform(transform)) { return "Pose contains an invalid transform"; }
            }
            return {};
        }

        std::string ValidateBlend(const Pose& base, const Pose& other, double weight, std::span<const double> mask)
        {
            if (const std::string error = ValidatePose(base); !error.empty()) { return error; }
            if (const std::string error = ValidatePose(other); !error.empty()) { return error; }
            if (base.localTransforms.size() != other.localTransforms.size()) { return "Pose bone counts differ"; }
            if (!std::isfinite(weight) || weight < 0.0 || weight > 1.0) { return "Blend weight must lie in [0,1]"; }
            if (!mask.empty() && mask.size() != base.localTransforms.size()) { return "Bone mask length must equal pose bone count"; }
            for (const double value : mask)
            {
                if (!std::isfinite(value) || value < 0.0 || value > 1.0) { return "Bone mask weights must lie in [0,1]"; }
            }
            return {};
        }
    }

    Quaternion Slerp(Quaternion from, Quaternion to, double alpha)
    {
        // Math primitive: callers validate rotations and alpha through the Result-returning APIs.
        from = Normalize(from);
        to = Normalize(to);
        double dot = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
        if (dot < 0.0)
        {
            to = { -to.x, -to.y, -to.z, -to.w };
            dot = -dot;
        }
        dot = std::clamp(dot, 0.0, 1.0);
        double fromWeight = 1.0 - alpha;
        double toWeight = alpha;
        if (dot < 0.9995)
        {
            const double angle = std::acos(dot);
            const double sine = std::sin(angle);
            fromWeight = std::sin((1.0 - alpha) * angle) / sine;
            toWeight = std::sin(alpha * angle) / sine;
        }
        return Normalize({ fromWeight * from.x + toWeight * to.x, fromWeight * from.y + toWeight * to.y,
            fromWeight * from.z + toWeight * to.z, fromWeight * from.w + toWeight * to.w });
    }

    std::string ValidateSkeleton(const Skeleton& skeleton)
    {
        if (!ValidName(skeleton.name)) { return "Skeleton name must contain 1 to 128 bytes without NUL"; }
        if (skeleton.bones.empty() || skeleton.bones.size() > MaxBones) { return "Skeleton must have 1 to 1024 bones"; }
        std::unordered_set<std::string> names;
        for (std::size_t index = 0; index < skeleton.bones.size(); ++index)
        {
            const Bone& bone = skeleton.bones[index];
            if (!ValidName(bone.name) || !names.insert(bone.name).second) { return "Bone names must be valid and unique"; }
            if (bone.parent < -1 || bone.parent >= static_cast<int>(skeleton.bones.size())) { return "Bone parent index is out of range"; }
            if (!ValidTransform(bone.bindPose)) { return "Bone bind pose is invalid: " + bone.name; }
            int current = static_cast<int>(index);
            std::size_t visited = 0;
            while (current != -1)
            {
                if (++visited > skeleton.bones.size()) { return "Skeleton parent graph contains a cycle"; }
                // Every parent must be checked here, including nodes later in the input order.
                if (current < 0 || current >= static_cast<int>(skeleton.bones.size())) { return "Bone parent index is out of range"; }
                current = skeleton.bones[static_cast<std::size_t>(current)].parent;
            }
        }
        return {};
    }

    std::string ValidateClip(const Clip& clip, const Skeleton& skeleton)
    {
        if (const std::string error = ValidateSkeleton(skeleton); !error.empty()) { return error; }
        if (!ValidName(clip.name)) { return "Clip name must contain 1 to 128 bytes without NUL"; }
        if (clip.skeletonName != skeleton.name) { return "Clip skeleton name does not match the supplied skeleton"; }
        if (const std::string error = Timing(clip, 0.0, {}); !error.empty()) { return error; }
        if (clip.tracks.size() > skeleton.bones.size()) { return "Clip has more tracks than skeleton bones"; }
        std::unordered_set<std::size_t> indices;
        std::size_t totalKeys = 0;
        for (const BoneTrack& track : clip.tracks)
        {
            if (track.boneIndex >= skeleton.bones.size() || !indices.insert(track.boneIndex).second)
            {
                return "Track bone index is out of range or duplicated";
            }
            if (track.keys.empty() || track.keys.size() > MaxKeysPerTrack) { return "Track must have 1 to 8192 keys"; }
            totalKeys += track.keys.size();
            if (totalKeys > MaxTotalKeys) { return "Clip exceeds the total keyframe limit"; }
            double last = -1.0;
            for (const Keyframe& key : track.keys)
            {
                if (!std::isfinite(key.time) || key.time < 0.0 || key.time > clip.duration || key.time <= last)
                {
                    return "Keyframe times must be strictly increasing inside the clip duration";
                }
                if (!ValidTransform(key.transform)) { return "Keyframe transform is invalid"; }
                if (key.interpolation != Interpolation::Step && key.interpolation != Interpolation::Linear
                    && key.interpolation != Interpolation::SmoothStep) { return "Unknown interpolation enum value"; }
                last = key.time;
            }
        }
        return ValidEvents(clip);
    }

    Result<Skeleton> ParseSkeleton(std::string_view json)
    {
        auto document = ReadJson(json);
        if (!document) { return Failure<Skeleton>(document.error); }
        const Json& object = document.value;
        if (const std::string error = Fields(object, { "formatVersion", "name", "bones" }, "skeleton"); !error.empty())
        {
            return Failure<Skeleton>(error);
        }
        Skeleton result;
        if (!Version(object)) { return Failure<Skeleton>("formatVersion must be integer 1"); }
        if (!Name(object, "name", result.name)) { return Failure<Skeleton>("Invalid skeleton name"); }
        const auto bones = object.find("bones");
        if (bones == object.end() || !bones->is_array() || bones->empty() || bones->size() > MaxBones)
        {
            return Failure<Skeleton>("bones must be an array with 1 to 1024 elements");
        }
        std::unordered_map<std::string, int> indices;
        std::vector<std::string> parentNames;
        result.bones.reserve(bones->size());
        for (const Json& input : *bones)
        {
            if (const std::string error = Fields(input, { "name", "parent", "translation", "rotation" }, "bone"); !error.empty())
            {
                return Failure<Skeleton>(error);
            }
            Bone bone;
            if (!Name(input, "name", bone.name)) { return Failure<Skeleton>("Invalid bone name"); }
            if (!indices.emplace(bone.name, static_cast<int>(result.bones.size())).second)
            {
                return Failure<Skeleton>("Duplicate bone name: " + bone.name);
            }
            const auto parent = input.find("parent");
            if (parent == input.end()) { return Failure<Skeleton>("Every bone requires parent: a bone name or null"); }
            std::string parentName;
            if (!parent->is_null())
            {
                if (!Name(input, "parent", parentName)) { return Failure<Skeleton>("Invalid bone parent name"); }
            }
            if (const std::string error = Transform(input, bone.bindPose); !error.empty()) { return Failure<Skeleton>(error); }
            result.bones.push_back(std::move(bone));
            parentNames.push_back(std::move(parentName));
        }
        for (std::size_t i = 0; i < result.bones.size(); ++i)
        {
            if (!parentNames[i].empty())
            {
                const auto found = indices.find(parentNames[i]);
                if (found == indices.end()) { return Failure<Skeleton>("Unknown parent bone: " + parentNames[i]); }
                result.bones[i].parent = found->second;
            }
        }
        if (const std::string error = ValidateSkeleton(result); !error.empty()) { return Failure<Skeleton>(error); }
        return { std::move(result), {} };
    }

    Result<Clip> ParseClip(std::string_view json, const Skeleton& skeleton)
    {
        if (const std::string error = ValidateSkeleton(skeleton); !error.empty()) { return Failure<Clip>(error); }
        auto document = ReadJson(json);
        if (!document) { return Failure<Clip>(document.error); }
        const Json& object = document.value;
        if (const std::string error = Fields(object,
            { "formatVersion", "name", "skeleton", "duration", "tracks", "events" }, "clip"); !error.empty())
        {
            return Failure<Clip>(error);
        }
        Clip result;
        if (!Version(object)) { return Failure<Clip>("formatVersion must be integer 1"); }
        if (!Name(object, "name", result.name) || !Name(object, "skeleton", result.skeletonName))
        {
            return Failure<Clip>("Invalid clip or skeleton name");
        }
        if (!Number(object, "duration", result.duration)) { return Failure<Clip>("duration must be a finite number"); }
        if (const std::string error = Timing(result, 0.0, {}); !error.empty()) { return Failure<Clip>(error); }
        if (result.skeletonName != skeleton.name) { return Failure<Clip>("Clip skeleton name does not match the supplied skeleton"); }
        const auto tracks = object.find("tracks");
        if (tracks == object.end() || !tracks->is_array() || tracks->size() > skeleton.bones.size())
        {
            return Failure<Clip>("tracks must be an array no larger than the skeleton");
        }
        std::unordered_map<std::string, std::size_t> indices;
        for (std::size_t i = 0; i < skeleton.bones.size(); ++i) { indices.emplace(skeleton.bones[i].name, i); }
        std::size_t totalKeys = 0;
        for (const Json& input : *tracks)
        {
            if (const std::string error = Fields(input, { "bone", "keys" }, "track"); !error.empty()) { return Failure<Clip>(error); }
            std::string name;
            if (!Name(input, "bone", name)) { return Failure<Clip>("Invalid track bone name"); }
            const auto bone = indices.find(name);
            if (bone == indices.end()) { return Failure<Clip>("Unknown track bone: " + name); }
            const auto keys = input.find("keys");
            if (keys == input.end() || !keys->is_array() || keys->empty() || keys->size() > MaxKeysPerTrack)
            {
                return Failure<Clip>("keys must be an array with 1 to 8192 elements");
            }
            totalKeys += keys->size();
            if (totalKeys > MaxTotalKeys) { return Failure<Clip>("Clip exceeds total keyframe limit"); }
            BoneTrack track;
            track.boneIndex = bone->second;
            track.keys.reserve(keys->size());
            for (const Json& inputKey : *keys)
            {
                if (const std::string error = Fields(inputKey, { "time", "translation", "rotation", "interpolation" }, "keyframe");
                    !error.empty()) { return Failure<Clip>(error); }
                Keyframe key;
                key.transform = skeleton.bones[track.boneIndex].bindPose;
                if (!Number(inputKey, "time", key.time)) { return Failure<Clip>("keyframe time must be a finite number"); }
                if (const std::string error = Transform(inputKey, key.transform); !error.empty()) { return Failure<Clip>(error); }
                if (const auto interpolation = inputKey.find("interpolation"); interpolation != inputKey.end())
                {
                    if (!interpolation->is_string()) { return Failure<Clip>("interpolation must be a string"); }
                    const std::string mode = interpolation->get<std::string>();
                    if (mode == "step") { key.interpolation = Interpolation::Step; }
                    else if (mode == "linear") { key.interpolation = Interpolation::Linear; }
                    else if (mode == "smoothstep") { key.interpolation = Interpolation::SmoothStep; }
                    else { return Failure<Clip>("Unknown interpolation: " + mode); }
                }
                track.keys.push_back(key);
            }
            result.tracks.push_back(std::move(track));
        }
        if (const auto events = object.find("events"); events != object.end())
        {
            if (!events->is_array() || events->size() > MaxEvents) { return Failure<Clip>("events must be an array of at most 4096 items"); }
            result.events.reserve(events->size());
            for (const Json& input : *events)
            {
                if (const std::string error = Fields(input, { "time", "name", "payload" }, "event"); !error.empty())
                {
                    return Failure<Clip>(error);
                }
                AnimationEvent event;
                if (!Number(input, "time", event.time) || !Name(input, "name", event.name)) { return Failure<Clip>("Invalid event time or name"); }
                if (const auto payload = input.find("payload"); payload != input.end())
                {
                    if (!payload->is_object()) { return Failure<Clip>("event payload must be an object"); }
                    event.payloadJson = payload->dump();
                }
                result.events.push_back(std::move(event));
            }
        }
        if (const std::string error = ValidateClip(result, skeleton); !error.empty()) { return Failure<Clip>(error); }
        return { std::move(result), {} };
    }

    Result<double> ResolveSampleTime(const Clip& clip, double timelineSeconds, PlaybackOptions options)
    {
        if (const std::string error = Timing(clip, timelineSeconds, options); !error.empty()) { return Failure<double>(error); }
        const double scaled = timelineSeconds * options.speed;
        return { options.loop ? std::fmod(scaled, clip.duration) : std::min(scaled, clip.duration), {} };
    }

    Result<Pose> EvaluatePose(const Clip& clip, const Skeleton& skeleton, double timelineSeconds, PlaybackOptions options)
    {
        if (const std::string error = ValidateClip(clip, skeleton); !error.empty()) { return Failure<Pose>(error); }
        const auto time = ResolveSampleTime(clip, timelineSeconds, options);
        if (!time) { return Failure<Pose>(time.error); }
        Pose pose;
        pose.localTransforms.reserve(skeleton.bones.size());
        for (const Bone& bone : skeleton.bones)
        {
            LocalTransform transform = bone.bindPose;
            transform.rotation = Normalize(transform.rotation);
            pose.localTransforms.push_back(transform);
        }
        for (const BoneTrack& track : clip.tracks)
        {
            LocalTransform& output = pose.localTransforms[track.boneIndex];
            if (time.value <= track.keys.front().time) { output = track.keys.front().transform; }
            else if (time.value >= track.keys.back().time) { output = track.keys.back().transform; }
            else
            {
                const auto right = std::upper_bound(track.keys.begin(), track.keys.end(), time.value,
                    [](double value, const Keyframe& key) { return value < key.time; });
                const auto left = right - 1;
                double alpha = (time.value - left->time) / (right->time - left->time);
                if (left->interpolation == Interpolation::Step) { alpha = 0.0; }
                else if (left->interpolation == Interpolation::SmoothStep) { alpha = alpha * alpha * (3.0 - 2.0 * alpha); }
                output = Interpolate(left->transform, right->transform, alpha);
            }
            output.rotation = Normalize(output.rotation);
        }
        return { std::move(pose), {} };
    }

    Result<std::vector<EventOccurrence>> CollectEvents(const Clip& clip, double fromTimelineSeconds,
        double toTimelineSeconds, PlaybackOptions options, std::size_t maxEvents)
    {
        if (const std::string error = Timing(clip, fromTimelineSeconds, options); !error.empty())
        {
            return Failure<std::vector<EventOccurrence>>(error);
        }
        if (const std::string error = Timing(clip, toTimelineSeconds, options); !error.empty())
        {
            return Failure<std::vector<EventOccurrence>>(error);
        }
        if (const std::string error = ValidEvents(clip); !error.empty()) { return Failure<std::vector<EventOccurrence>>(error); }
        if (toTimelineSeconds < fromTimelineSeconds) { return Failure<std::vector<EventOccurrence>>("Event interval must move forward"); }
        if (maxEvents > MaxCollectedEvents) { return Failure<std::vector<EventOccurrence>>("Event budget exceeds 65536"); }
        std::vector<EventOccurrence> result;
        if (fromTimelineSeconds == toTimelineSeconds || options.speed == 0.0 || clip.events.empty()) { return { {}, {} }; }
        const long double from = static_cast<long double>(fromTimelineSeconds) * options.speed;
        const long double to = static_cast<long double>(toTimelineSeconds) * options.speed;
        const long double duration = clip.duration;
        for (std::size_t index = 0; index < clip.events.size(); ++index)
        {
            const long double eventTime = clip.events[index].time;
            if (!options.loop)
            {
                if (eventTime > from && eventTime <= to)
                {
                    if (result.size() == maxEvents) { return Failure<std::vector<EventOccurrence>>("Event budget exceeded"); }
                    result.push_back({ index, static_cast<double>(eventTime / options.speed) });
                }
                continue;
            }
            const long double first = std::max(0.0L, std::floor((from - eventTime) / duration) + 1.0L);
            const long double last = std::floor((to - eventTime) / duration);
            if (last < first) { continue; }
            const long double count = last - first + 1.0L;
            if (count > static_cast<long double>(maxEvents - result.size()))
            {
                return Failure<std::vector<EventOccurrence>>("Event budget exceeded");
            }
            // Bounded count, not the potentially huge loop index, drives the iteration.
            const std::size_t occurrences = static_cast<std::size_t>(count);
            for (std::size_t occurrence = 0; occurrence < occurrences; ++occurrence)
            {
                const long double absolute = eventTime + (first + static_cast<long double>(occurrence)) * duration;
                result.push_back({ index, static_cast<double>(absolute / options.speed) });
            }
        }
        std::sort(result.begin(), result.end(), [](const EventOccurrence& a, const EventOccurrence& b)
        {
            return a.timelineSeconds < b.timelineSeconds || (a.timelineSeconds == b.timelineSeconds && a.eventIndex < b.eventIndex);
        });
        return { std::move(result), {} };
    }

    Result<Pose> BlendPoses(const Pose& base, const Pose& other, double weight, std::span<const double> boneMask)
    {
        if (const std::string error = ValidateBlend(base, other, weight, boneMask); !error.empty()) { return Failure<Pose>(error); }
        Pose output = base;
        for (std::size_t i = 0; i < output.localTransforms.size(); ++i)
        {
            const double effective = weight * (boneMask.empty() ? 1.0 : boneMask[i]);
            output.localTransforms[i] = Interpolate(base.localTransforms[i], other.localTransforms[i], effective);
        }
        return { std::move(output), {} };
    }

    Result<Pose> ApplyAdditiveLayer(const Pose& base, const Pose& layer, const Pose& reference, double weight,
        std::span<const double> boneMask)
    {
        if (const std::string error = ValidateBlend(base, layer, weight, boneMask); !error.empty()) { return Failure<Pose>(error); }
        if (const std::string error = ValidateBlend(base, reference, weight, boneMask); !error.empty()) { return Failure<Pose>(error); }
        Pose output = base;
        for (std::size_t i = 0; i < output.localTransforms.size(); ++i)
        {
            const double effective = weight * (boneMask.empty() ? 1.0 : boneMask[i]);
            const LocalTransform& input = layer.localTransforms[i];
            const LocalTransform& rest = reference.localTransforms[i];
            LocalTransform& transform = output.localTransforms[i];
            transform.translation.x += (input.translation.x - rest.translation.x) * effective;
            transform.translation.y += (input.translation.y - rest.translation.y) * effective;
            transform.translation.z += (input.translation.z - rest.translation.z) * effective;
            const Quaternion restRotation = Normalize(rest.rotation);
            const Quaternion inverse{ -restRotation.x, -restRotation.y, -restRotation.z, restRotation.w };
            const Quaternion delta = Multiply(Normalize(input.rotation), inverse);
            transform.rotation = Normalize(Multiply(Slerp({}, delta, effective), Normalize(transform.rotation)));
            if (!ValidTransform(transform)) { return Failure<Pose>("Additive layer exceeds supported transform bounds"); }
        }
        return { std::move(output), {} };
    }
}
