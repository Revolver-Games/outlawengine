/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/ScriptMotion.h>
#include <ScriptMotion/MotionBake.h>
#include <algorithm>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace SM = Wanted::ScriptMotion;

    class Failure : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    void Require(bool condition, std::string_view explanation)
    {
        if (!condition)
        {
            throw Failure(std::string(explanation));
        }
    }

    void Near(double actual, double expected, std::string_view explanation, double tolerance = 1e-9)
    {
        if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        {
            throw Failure(std::string(explanation) + ": expected " + std::to_string(expected)
                + ", got " + std::to_string(actual));
        }
    }

    void VectorNear(const SM::Vec3& actual, const SM::Vec3& expected)
    {
        Near(actual.x, expected.x, "translation X");
        Near(actual.y, expected.y, "translation Y");
        Near(actual.z, expected.z, "translation Z");
    }

    void RotationNear(const SM::Quaternion& actual, const SM::Quaternion& expected)
    {
        const double normSquared = actual.x * actual.x + actual.y * actual.y
            + actual.z * actual.z + actual.w * actual.w;
        Near(normSquared, 1.0, "output quaternion must be unit length");
        const double dot = actual.x * expected.x + actual.y * expected.y
            + actual.z * expected.z + actual.w * expected.w;
        Near(std::abs(dot), 1.0, "quaternion must represent expected orientation");
    }

    template<class T>
    T Must(SM::Result<T> result)
    {
        if (!result)
        {
            throw Failure("unexpected error: " + result.error);
        }
        return std::move(result.value);
    }

    template<class T>
    void Reject(const SM::Result<T>& result)
    {
        Require(!result, "invalid input must fail");
        Require(!result.error.empty(), "failure must explain the invalid input");
    }

    constexpr std::string_view SkeletonJson = R"json({
        "formatVersion":1,
        "name":"test_humanoid",
        "bones":[
            {"name":"root","parent":null,"translation":[0,0,0],"rotation":[0,0,0,1]},
            {"name":"spine","parent":"root","translation":[0,0,1],"rotation":[0,0,0,1]},
            {"name":"hand","parent":"spine","translation":[1,0,0],"rotation":[0,0,0,1]}
        ]
    })json";

    constexpr std::string_view ClipJson = R"json({
        "formatVersion":1,
        "name":"move_and_turn",
        "skeleton":"test_humanoid",
        "duration":2,
        "tracks":[{
            "bone":"root",
            "keys":[
                {"time":0,"translation":[0,0,0],"rotation":[0,0,0,1],"interpolation":"linear"},
                {"time":2,"translation":[2,4,6],"rotation":[0,0,1,0]}
            ]
        }],
        "events":[
            {"time":0,"name":"begin"},
            {"time":0.5,"name":"footstep","payload":{"material":"wood","volume":0.75}},
            {"time":2,"name":"end"}
        ]
    })json";

    SM::Skeleton TestSkeleton()
    {
        return Must(SM::ParseSkeleton(SkeletonJson));
    }

    SM::Clip TestClip()
    {
        return Must(SM::ParseClip(ClipJson, TestSkeleton()));
    }

    std::string ReplaceOnce(std::string_view source, std::string_view oldText, std::string_view newText)
    {
        std::string result(source);
        const auto offset = result.find(oldText);
        Require(offset != std::string::npos, "test fixture replacement must find original text");
        result.replace(offset, oldText.size(), newText);
        return result;
    }

    SM::Pose SinglePose(SM::Vec3 translation, SM::Quaternion rotation = {})
    {
        return SM::Pose{{SM::LocalTransform{translation, rotation}}};
    }

    using Test = std::pair<std::string, std::function<void()>>;

    std::vector<Test> Tests()
    {
        std::vector<Test> tests;
        auto add = [&tests](std::string name, std::function<void()> body)
        {
            tests.emplace_back(std::move(name), std::move(body));
        };

        add("parse.skeleton_valid_parent_mapping", []
        {
            const auto skeleton = TestSkeleton();
            Require(skeleton.name == "test_humanoid", "skeleton name round trip");
            Require(skeleton.bones.size() == 3, "three skeleton bones");
            Require(skeleton.bones[0].parent == -1, "root has no parent");
            Require(skeleton.bones[1].parent == 0, "spine parent maps to root");
            Require(skeleton.bones[2].parent == 1, "hand parent maps to spine");
            VectorNear(skeleton.bones[2].bindPose.translation, {1, 0, 0});
        });

        add("parse.clip_valid_track_events_payload", []
        {
            const auto clip = TestClip();
            Require(clip.name == "move_and_turn", "clip name round trip");
            Require(clip.skeletonName == "test_humanoid", "skeleton identity round trip");
            Near(clip.duration, 2, "clip duration");
            Require(clip.tracks.size() == 1 && clip.tracks[0].boneIndex == 0, "track bone resolution");
            Require(clip.tracks[0].keys.size() == 2, "key count");
            Require(clip.events.size() == 3, "event count");
            Require(clip.events[1].payloadJson.find("wood") != std::string::npos, "event payload is retained");
        });

        add("parse.defaults_and_bind_channel_fallback", []
        {
            const auto skeleton = TestSkeleton();
            const auto clip = Must(SM::ParseClip(R"json({"formatVersion":1,"name":"partial","skeleton":"test_humanoid","duration":2,"tracks":[{"bone":"hand","keys":[{"time":0,"translation":[9,8,7]},{"time":2,"rotation":[0,0,1,0]}]}]})json", skeleton));
            const auto initial = Must(SM::EvaluatePose(clip, skeleton, 0, {1, false}));
            const auto final = Must(SM::EvaluatePose(clip, skeleton, 2, {1, false}));
            VectorNear(initial.localTransforms[2].translation, {9, 8, 7});
            RotationNear(initial.localTransforms[2].rotation, {});
            VectorNear(final.localTransforms[2].translation, {1, 0, 0});
            VectorNear(final.localTransforms[1].translation, {0, 0, 1});
            Require(clip.events.empty(), "omitted events default to empty");
        });

        add("parse.skeleton_identity_defaults", []
        {
            const auto skeleton = Must(SM::ParseSkeleton(R"({"formatVersion":1,"name":"minimal","bones":[{"name":"root","parent":null}]})"));
            VectorNear(skeleton.bones[0].bindPose.translation, {});
            RotationNear(skeleton.bones[0].bindPose.rotation, {});
        });

        add("parse.normalizes_nonzero_quaternions", []
        {
            const auto skeleton = Must(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "[0,0,0,1]", "[0,0,0,4]")));
            RotationNear(skeleton.bones[0].bindPose.rotation, {});
            const auto clip = Must(SM::ParseClip(ReplaceOnce(ClipJson, "[0,0,1,0]", "[0,0,2,0]"), skeleton));
            RotationNear(clip.tracks[0].keys[1].transform.rotation, {0, 0, 1, 0});
        });

        add("parse.rejects_malformed_or_wrong_root_json", []
        {
            for (const auto invalid : {"", "{", "[]", "null", "true", "{} trailing", "{\"x\":NaN}"})
            {
                Reject(SM::ParseSkeleton(invalid));
                Reject(SM::ParseClip(invalid, TestSkeleton()));
            }
        });

        add("parse.rejects_duplicate_keys", []
        {
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"name\":\"test_humanoid\"", "\"name\":\"first\",\"name\":\"test_humanoid\"")));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"time\":0,", "\"time\":0,\"time\":1,"), TestSkeleton()));
        });

        add("parse.rejects_unknown_fields", []
        {
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"formatVersion\":1,", "\"formatVersion\":1,\"typo\":true,")));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"interpolation\":\"linear\"", "\"interpolation\":\"linear\",\"rotatoin\":[0,0,0,1]"), TestSkeleton()));
        });

        add("parse.rejects_unsupported_version_and_types", []
        {
            for (const auto replacement : {"2", "0", "1.5", "\"1\"", "null", "true"})
            {
                Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"formatVersion\":1", std::string("\"formatVersion\":") + replacement)));
                Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"formatVersion\":1", std::string("\"formatVersion\":") + replacement), TestSkeleton()));
            }
        });

        add("parse.rejects_missing_required_fields", []
        {
            Reject(SM::ParseSkeleton(R"({"formatVersion":1,"bones":[]})"));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"duration\":2,", ""), TestSkeleton()));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"bone\":\"root\",", ""), TestSkeleton()));
        });

        add("parse.rejects_zero_or_wrong_size_transforms", []
        {
            for (const auto replacement : {"[0,0,0,0]", "[0,0,1]", "[0,0,0,1,2]", "[0,0,0,\"1\"]"})
            {
                Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "[0,0,0,1]", replacement)));
                Reject(SM::ParseClip(ReplaceOnce(ClipJson, "[0,0,0,1]", replacement), TestSkeleton()));
            }
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "[0,0,0]", "[0,0]")));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "[2,4,6]", "[2,4,null]"), TestSkeleton()));
        });

        add("parse.rejects_overflow_numeric_values", []
        {
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "[0,0,0]", "[1e9999,0,0]")));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"duration\":2", "\"duration\":1e9999"), TestSkeleton()));
        });

        add("parse.rejects_duplicate_unknown_and_cyclic_bones", []
        {
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"name\":\"hand\"", "\"name\":\"root\"")));
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"parent\":\"spine\"", "\"parent\":\"missing\"")));
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"parent\":null", "\"parent\":\"hand\"")));
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "\"parent\":\"spine\"", "\"parent\":\"hand\"")));
        });

        add("parse.rejects_wrong_skeleton_or_unknown_track", []
        {
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"skeleton\":\"test_humanoid\"", "\"skeleton\":\"other\""), TestSkeleton()));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"bone\":\"root\"", "\"bone\":\"missing\""), TestSkeleton()));
        });

        add("parse.rejects_invalid_duration_and_key_timing", []
        {
            for (const auto replacement : {"0", "-2", "\"2\""})
            {
                Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"duration\":2", std::string("\"duration\":") + replacement), TestSkeleton()));
            }
            for (const auto replacement : {"-0.1", "0", "2.1"})
            {
                Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"time\":2,", std::string("\"time\":") + replacement + ","), TestSkeleton()));
            }
        });

        add("parse.rejects_unknown_interpolation", []
        {
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"linear\"", "\"cubic_typo\""), TestSkeleton()));
        });

        add("parse.rejects_event_time_outside_clip", []
        {
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"time\":0.5,\"name\":\"footstep\"", "\"time\":2.5,\"name\":\"footstep\""), TestSkeleton()));
            Reject(SM::ParseClip(ReplaceOnce(ClipJson, "\"time\":0,\"name\":\"begin\"", "\"time\":-1,\"name\":\"begin\""), TestSkeleton()));
        });

        add("parse.rejects_oversized_document", []
        {
            const std::string oversized(SM::MaxJsonBytes + 1, ' ');
            Reject(SM::ParseSkeleton(oversized));
            Reject(SM::ParseClip(oversized, TestSkeleton()));
        });

        add("parse.rejects_excessive_nesting_without_aborting", []
        {
            std::string nested(64, '[');
            nested += '0';
            nested.append(64, ']');
            const auto input = ReplaceOnce(ClipJson,
                "\"payload\":{\"material\":\"wood\",\"volume\":0.75}",
                "\"payload\":{\"deep\":" + nested + "}");
            Reject(SM::ParseClip(input, TestSkeleton()));
        });

        add("parse.unicode_escape_validation_and_decoding", []
        {
            const auto unicode = ReplaceOnce(SkeletonJson, "test_humanoid", "test_\\u00e9_\\ud83d\\udc0e");
            const auto skeleton = Must(SM::ParseSkeleton(unicode));
            Require(skeleton.name == "test_\xc3\xa9_\xf0\x9f\x90\x8e", "Unicode and surrogate pairs decode to UTF-8");
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "test_humanoid", "bad_\\ud800")));
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "test_humanoid", "bad_\\u0000")));
            const std::string invalidUtf8{"bad_\xc0\xaf", 6};
            Reject(SM::ParseSkeleton(ReplaceOnce(SkeletonJson, "test_humanoid", invalidUtf8)));
        });

        add("validate.rejects_invalid_runtime_skeleton", []
        {
            auto skeleton = TestSkeleton();
            skeleton.bones[1].parent = 100;
            Require(!SM::ValidateSkeleton(skeleton).empty(), "out-of-bounds parent rejects");
            skeleton = TestSkeleton();
            skeleton.bones[0].parent = 2;
            Require(!SM::ValidateSkeleton(skeleton).empty(), "cycle rejects");
            skeleton = TestSkeleton();
            skeleton.bones[0].bindPose.translation.x = std::numeric_limits<double>::infinity();
            Require(!SM::ValidateSkeleton(skeleton).empty(), "infinite translation rejects");
        });

        add("validate.enforces_skeleton_bone_limit", []
        {
            auto skeleton = TestSkeleton();
            while (skeleton.bones.size() <= SM::MaxBones)
            {
                skeleton.bones.push_back(SM::Bone{"bone" + std::to_string(skeleton.bones.size()), 0, {}});
            }
            Require(!SM::ValidateSkeleton(skeleton).empty(), "oversized skeleton rejects");
        });

        add("validate.rejects_duplicate_tracks_and_keys", []
        {
            auto clip = TestClip();
            clip.tracks.push_back(clip.tracks.front());
            Require(!SM::ValidateClip(clip, TestSkeleton()).empty(), "duplicate track rejects");
            clip = TestClip();
            clip.tracks[0].keys[1].time = 0;
            Require(!SM::ValidateClip(clip, TestSkeleton()).empty(), "duplicate key time rejects");
            clip = TestClip();
            clip.tracks[0].keys.clear();
            Require(!SM::ValidateClip(clip, TestSkeleton()).empty(), "empty track rejects");
        });

        add("pose.linear_translation_and_slerp_midpoint", []
        {
            const auto pose = Must(SM::EvaluatePose(TestClip(), TestSkeleton(), 1, {1, false}));
            const double halfSqrt = std::sqrt(0.5);
            VectorNear(pose.localTransforms[0].translation, {1, 2, 3});
            RotationNear(pose.localTransforms[0].rotation, {0, 0, halfSqrt, halfSqrt});
        });

        add("pose.exact_endpoints_and_bind_fallback", []
        {
            const auto start = Must(SM::EvaluatePose(TestClip(), TestSkeleton(), 0, {1, false}));
            const auto end = Must(SM::EvaluatePose(TestClip(), TestSkeleton(), 2, {1, false}));
            VectorNear(start.localTransforms[0].translation, {});
            RotationNear(start.localTransforms[0].rotation, {});
            VectorNear(end.localTransforms[0].translation, {2, 4, 6});
            RotationNear(end.localTransforms[0].rotation, {0, 0, 1, 0});
            VectorNear(end.localTransforms[1].translation, {0, 0, 1});
            VectorNear(end.localTransforms[2].translation, {1, 0, 0});
        });

        add("pose.step_changes_at_exact_key", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys[0].interpolation = SM::Interpolation::Step;
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 1.999, {1, false})).localTransforms[0].translation, {});
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 2, {1, false})).localTransforms[0].translation, {2, 4, 6});
        });

        add("pose.smoothstep_known_quarter_value", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys[0].interpolation = SM::Interpolation::SmoothStep;
            const auto pose = Must(SM::EvaluatePose(clip, TestSkeleton(), 0.5, {1, false}));
            // At u=1/4, 3u^2 - 2u^3 is exactly 5/32.
            VectorNear(pose.localTransforms[0].translation, {0.3125, 0.625, 0.9375});
            const double halfAngle = std::numbers::pi * 5.0 / 64.0;
            RotationNear(pose.localTransforms[0].rotation, {0, 0, std::sin(halfAngle), std::cos(halfAngle)});
        });

        add("pose.single_key_and_sparse_endpoint_holds", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys.erase(clip.tracks[0].keys.begin());
            clip.tracks[0].keys[0].time = 1;
            for (const auto time : {0.0, 0.5, 1.0, 2.0, 100.0})
            {
                VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), time, {1, false})).localTransforms[0].translation, {2, 4, 6});
            }
        });

        add("pose.selects_interior_segment", []
        {
            auto clip = TestClip();
            SM::Keyframe middle;
            middle.time = 1;
            middle.transform.translation = {10, 0, 0};
            clip.tracks[0].keys.insert(clip.tracks[0].keys.begin() + 1, middle);
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 0.5, {1, false})).localTransforms[0].translation, {5, 0, 0});
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 1, {1, false})).localTransforms[0].translation, {10, 0, 0});
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 1.5, {1, false})).localTransforms[0].translation, {6, 2, 3});
        });

        add("pose.rejects_corrupt_programmatic_clip", []
        {
            auto clip = TestClip();
            clip.tracks[0].boneIndex = 10000;
            Reject(SM::EvaluatePose(clip, TestSkeleton(), 1));
            clip = TestClip();
            clip.tracks[0].keys[1].transform.rotation = {0, 0, 0, 0};
            Reject(SM::EvaluatePose(clip, TestSkeleton(), 1));
        });

        add("slerp.shortest_path_270_to_minus90", []
        {
            const double halfSqrt = std::sqrt(0.5);
            const auto halfway = SM::Slerp({}, {0, 0, halfSqrt, -halfSqrt}, 0.5);
            RotationNear(halfway, {0, 0, -std::sin(std::numbers::pi / 8.0), std::cos(std::numbers::pi / 8.0)});
        });

        add("slerp.opposite_sign_same_rotation_is_stable", []
        {
            const double halfSqrt = std::sqrt(0.5);
            const SM::Quaternion q{halfSqrt, 0, 0, halfSqrt};
            const SM::Quaternion negative{-halfSqrt, 0, 0, -halfSqrt};
            for (const auto alpha : {0.0, 0.25, 0.5, 0.75, 1.0})
            {
                RotationNear(SM::Slerp(q, negative, alpha), q);
            }
        });

        add("slerp.nearly_identical_and_nonunit_inputs", []
        {
            RotationNear(SM::Slerp({0, 0, 0, 4}, {0, 0, 0, 2}, 0.25), {});
            const double theta = 1e-5;
            RotationNear(SM::Slerp({}, {0, 0, std::sin(theta), std::cos(theta)}, 0.5),
                {0, 0, std::sin(theta / 2), std::cos(theta / 2)});
        });

        add("time.loop_boundary_and_clamped_boundary", []
        {
            const auto clip = TestClip();
            Near(Must(SM::ResolveSampleTime(clip, 0)), 0, "initial loop time");
            Near(Must(SM::ResolveSampleTime(clip, 2)), 0, "exact duration wraps");
            Near(Must(SM::ResolveSampleTime(clip, 5.25)), 1.25, "multiple loops wrap");
            Near(Must(SM::ResolveSampleTime(clip, 2, {1, false})), 2, "clamp keeps final key");
            Near(Must(SM::ResolveSampleTime(clip, 500, {1, false})), 2, "past end clamps");
        });

        add("time.speed_and_pause", []
        {
            const auto clip = TestClip();
            Near(Must(SM::ResolveSampleTime(clip, 0.75, {2, false})), 1.5, "double playback speed");
            Near(Must(SM::ResolveSampleTime(clip, 3, {0.5, true})), 1.5, "half playback speed");
            Near(Must(SM::ResolveSampleTime(clip, 100, {0, true})), 0, "zero speed stays at start");
            VectorNear(Must(SM::EvaluatePose(clip, TestSkeleton(), 0.25, {2, false})).localTransforms[0].translation, {0.5, 1, 1.5});
        });

        add("time.rejects_negative_nan_infinite_and_overflow", []
        {
            const auto clip = TestClip();
            for (const auto invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
            {
                Reject(SM::ResolveSampleTime(clip, invalid));
                Reject(SM::ResolveSampleTime(clip, 1, {invalid, true}));
                Reject(SM::EvaluatePose(clip, TestSkeleton(), invalid));
            }
            Reject(SM::ResolveSampleTime(clip, std::numeric_limits<double>::max(), {2, true}));
        });

        add("time.rejects_loop_counts_beyond_portable_precision", []
        {
            auto clip = TestClip();
            clip.duration = 1e-6;
            clip.tracks[0].keys[1].time = clip.duration;
            clip.events.clear();
            Require(SM::ValidateClip(clip, TestSkeleton()).empty(), "tiny clip itself is valid");
            static_cast<void>(Must(SM::ResolveSampleTime(clip, 0.25, {1, true})));
            Reject(SM::ResolveSampleTime(clip, 1e9, {100, true}));
            // Empty events ensure rejection comes from timing validation, not event-budget exhaustion.
            Reject(SM::CollectEvents(clip, 0, 1e9, {100, true}));
        });

        add("events.open_closed_interval_and_no_start_event", []
        {
            const auto first = Must(SM::CollectEvents(TestClip(), 0, 0.5, {1, false}));
            Require(first.size() == 1 && first[0].eventIndex == 1, "(0,.5] includes only footstep");
            Near(first[0].timelineSeconds, 0.5, "event timeline");
            const auto second = Must(SM::CollectEvents(TestClip(), 0.5, 2, {1, false}));
            Require(second.size() == 1 && second[0].eventIndex == 2, "(.5,2] excludes preceding footstep");
            Require(Must(SM::CollectEvents(TestClip(), 0.5, 0.5)).empty(), "empty interval emits nothing");
        });

        add("events.multiple_loops_and_shared_boundary_order", []
        {
            const auto events = Must(SM::CollectEvents(TestClip(), 0, 4, {1, true}));
            const std::vector<std::size_t> indices{1, 0, 2, 1, 0, 2};
            const std::vector<double> times{0.5, 2, 2, 2.5, 4, 4};
            Require(events.size() == indices.size(), "all loop crossings are emitted");
            for (std::size_t i = 0; i < events.size(); ++i)
            {
                Require(events[i].eventIndex == indices[i], "equal-time events use source-index ordering");
                Near(events[i].timelineSeconds, times[i], "loop event absolute timeline");
            }
        });

        add("events.frame_partition_does_not_duplicate", []
        {
            const auto clip = TestClip();
            const auto all = Must(SM::CollectEvents(clip, 0, 4));
            auto split = Must(SM::CollectEvents(clip, 0, 2));
            const auto second = Must(SM::CollectEvents(clip, 2, 4));
            split.insert(split.end(), second.begin(), second.end());
            Require(all.size() == split.size(), "frame partition preserves event count");
            for (std::size_t i = 0; i < all.size(); ++i)
            {
                Require(all[i].eventIndex == split[i].eventIndex, "frame partition preserves event identity");
                Near(split[i].timelineSeconds, all[i].timelineSeconds, "frame partition preserves event time");
            }
        });

        add("events.speed_maps_back_to_wall_timeline", []
        {
            const auto events = Must(SM::CollectEvents(TestClip(), 0, 1, {2, false}));
            Require(events.size() == 2, "double speed traverses whole clip in one second");
            Near(events[0].timelineSeconds, 0.25, "footstep occurs twice as soon");
            Near(events[1].timelineSeconds, 1, "end event occurs twice as soon");
            Require(Must(SM::CollectEvents(TestClip(), 0, 100, {0, true})).empty(), "paused playback emits nothing");
        });

        add("events.same_timestamp_preserves_author_order", []
        {
            auto clip = TestClip();
            clip.events = {{1, "first", "{}"}, {1, "second", "{}"}};
            const auto events = Must(SM::CollectEvents(clip, 0, 1, {1, false}));
            Require(events.size() == 2 && events[0].eventIndex == 0 && events[1].eventIndex == 1,
                "same timestamp preserves source order");
        });

        add("events.budget_is_atomic", []
        {
            const auto exact = Must(SM::CollectEvents(TestClip(), 0, 4, {1, true}, 6));
            Require(exact.size() == 6, "exact budget succeeds");
            const auto limited = SM::CollectEvents(TestClip(), 0, 4, {1, true}, 5);
            Reject(limited);
            Require(limited.value.empty(), "budget failure cannot expose partial event batch");
            Reject(SM::CollectEvents(TestClip(), 0, 1e100, {1, true}, 10));
        });

        add("events.rejects_invalid_intervals_and_event_data", []
        {
            Reject(SM::CollectEvents(TestClip(), 2, 1));
            Reject(SM::CollectEvents(TestClip(), -1, 1));
            Reject(SM::CollectEvents(TestClip(), 0, std::numeric_limits<double>::quiet_NaN()));
            auto clip = TestClip();
            clip.events[1].time = -1;
            Reject(SM::CollectEvents(clip, 0, 1));
        });

        add("blend.weight_endpoints_midpoint_and_rotation", []
        {
            const auto base = SinglePose({1, 2, 3});
            const auto other = SinglePose({5, 10, 15}, {0, 0, 1, 0});
            VectorNear(Must(SM::BlendPoses(base, other, 0)).localTransforms[0].translation, {1, 2, 3});
            VectorNear(Must(SM::BlendPoses(base, other, 1)).localTransforms[0].translation, {5, 10, 15});
            const auto mid = Must(SM::BlendPoses(base, other, 0.5));
            VectorNear(mid.localTransforms[0].translation, {3, 6, 9});
            const double halfSqrt = std::sqrt(0.5);
            RotationNear(mid.localTransforms[0].rotation, {0, 0, halfSqrt, halfSqrt});
        });

        add("blend.mask_weights_independently", []
        {
            const SM::Pose base{{SM::LocalTransform{}, SM::LocalTransform{}, SM::LocalTransform{}}};
            const SM::Pose other{{SM::LocalTransform{{8, 0, 0}, {}}, SM::LocalTransform{{8, 0, 0}, {}}, SM::LocalTransform{{8, 0, 0}, {}}}};
            const std::vector<double> mask{0, 0.5, 1};
            const auto mixed = Must(SM::BlendPoses(base, other, 0.5, mask));
            VectorNear(mixed.localTransforms[0].translation, {});
            VectorNear(mixed.localTransforms[1].translation, {2, 0, 0});
            VectorNear(mixed.localTransforms[2].translation, {4, 0, 0});
        });

        add("blend.rejects_shape_weight_and_nonfinite_pose", []
        {
            const auto pose = SinglePose({});
            Reject(SM::BlendPoses(pose, SM::Pose{}, 0.5));
            const std::vector<double> tooLong{1, 1};
            Reject(SM::BlendPoses(pose, pose, 0.5, tooLong));
            for (const auto invalid : {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN()})
            {
                Reject(SM::BlendPoses(pose, pose, invalid));
                const std::vector<double> mask{invalid};
                Reject(SM::BlendPoses(pose, pose, 0.5, mask));
            }
            Reject(SM::BlendPoses(pose, SinglePose({std::numeric_limits<double>::infinity(), 0, 0}), 0.5));
            Reject(SM::BlendPoses(pose, SinglePose({}, {0, 0, 0, 0}), 0.5));
        });

        add("additive.translation_delta_and_identity_reference", []
        {
            const auto base = SinglePose({10, 20, 30});
            const auto reference = SinglePose({1, 2, 3});
            const auto layer = SinglePose({5, 10, 15}, {0, 0, 1, 0});
            const auto result = Must(SM::ApplyAdditiveLayer(base, layer, reference, 0.5));
            VectorNear(result.localTransforms[0].translation, {12, 24, 36});
            const double halfSqrt = std::sqrt(0.5);
            RotationNear(result.localTransforms[0].rotation, {0, 0, halfSqrt, halfSqrt});
            const auto unchanged = Must(SM::ApplyAdditiveLayer(base, reference, reference, 1));
            VectorNear(unchanged.localTransforms[0].translation, {10, 20, 30});
            RotationNear(unchanged.localTransforms[0].rotation, {});
        });

        add("additive.parent_space_rotation_order", []
        {
            const double halfSqrt = std::sqrt(0.5);
            // Apply a parent-space 90-degree Z delta to a base 90-degree X turn.
            // Hamilton product Z*X is (.5,.5,.5,.5); X*Z would have negative Y.
            const auto base = SinglePose({}, {halfSqrt, 0, 0, halfSqrt});
            const auto layer = SinglePose({}, {0, 0, halfSqrt, halfSqrt});
            const auto result = Must(SM::ApplyAdditiveLayer(base, layer, SinglePose({}), 1));
            RotationNear(result.localTransforms[0].rotation, {0.5, 0.5, 0.5, 0.5});
        });

        add("additive.nonidentity_reference_and_mask", []
        {
            const double halfSqrt = std::sqrt(0.5);
            const auto base = SinglePose({4, 5, 6});
            const auto reference = SinglePose({}, {0, 0, halfSqrt, halfSqrt});
            const auto layer = SinglePose({8, 0, 0}, {0, 0, 1, 0});
            const std::vector<double> mask{0.5};
            const auto result = Must(SM::ApplyAdditiveLayer(base, layer, reference, 1, mask));
            VectorNear(result.localTransforms[0].translation, {8, 5, 6});
            RotationNear(result.localTransforms[0].rotation, {0, 0, std::sin(std::numbers::pi / 8), std::cos(std::numbers::pi / 8)});
        });

        add("additive.rejects_mismatched_reference", []
        {
            const auto pose = SinglePose({});
            Reject(SM::ApplyAdditiveLayer(pose, pose, SM::Pose{}, 0.5));
        });

        add("model_space.parent_rotation_and_translation", []
        {
            const auto skeleton = TestSkeleton();
            SM::Pose local{{
                {{10, 0, 0}, {0, 0, std::sqrt(0.5), std::sqrt(0.5)}},
                {{0, 2, 0}, {}},
                {{1, 0, 0}, {}}
            }};
            const auto model = Must(SM::ComputeModelSpacePose(skeleton, local));
            VectorNear(model.localTransforms[0].translation, {10, 0, 0});
            VectorNear(model.localTransforms[1].translation, {8, 0, 0});
            VectorNear(model.localTransforms[2].translation, {8, 1, 0});
            RotationNear(model.localTransforms[2].rotation, local.localTransforms[0].rotation);
        });

        add("model_space.unordered_parent_indices", []
        {
            SM::Skeleton skeleton{"unordered", {
                {"leaf", 1, {}},
                {"middle", 2, {}},
                {"root", -1, {}}
            }};
            SM::Pose local{{
                {{1, 0, 0}, {}},
                {{2, 0, 0}, {}},
                {{3, 0, 0}, {}}
            }};
            const auto model = Must(SM::ComputeModelSpacePose(skeleton, local));
            VectorNear(model.localTransforms[0].translation, {6, 0, 0});
            VectorNear(model.localTransforms[1].translation, {5, 0, 0});
            VectorNear(model.localTransforms[2].translation, {3, 0, 0});
        });

        add("model_space.rejects_invalid_pose_or_skeleton", []
        {
            auto skeleton = TestSkeleton();
            Reject(SM::ComputeModelSpacePose(skeleton, SM::Pose{}));
            SM::Pose pose{{SM::LocalTransform{}, SM::LocalTransform{}, SM::LocalTransform{}}};
            pose.localTransforms[0].translation.x = std::numeric_limits<double>::infinity();
            Reject(SM::ComputeModelSpacePose(skeleton, pose));
            pose.localTransforms[0].translation.x = 0;
            skeleton.bones[0].parent = 2;
            Reject(SM::ComputeModelSpacePose(skeleton, pose));
        });

        add("model_space.clip_integration_matches_local_assembly", []
        {
            const auto skeleton = TestSkeleton();
            const auto clip = TestClip();
            const auto local = Must(SM::EvaluatePose(clip, skeleton, 0.5, {1, false}));
            const auto a = Must(SM::ComputeModelSpacePose(skeleton, local));
            const auto b = Must(SM::EvaluateModelSpacePose(clip, skeleton, 0.5, {1, false}));
            for (std::size_t i = 0; i < skeleton.bones.size(); ++i)
            {
                VectorNear(a.localTransforms[i].translation, b.localTransforms[i].translation);
                RotationNear(a.localTransforms[i].rotation, b.localTransforms[i].rotation);
            }
        });

        add("bake.endpoints_and_untracked_bind_joints", []
        {
            const auto skeleton = TestSkeleton();
            const auto bake = Must(SM::BakeMotion(TestClip(), skeleton, 30));
            Require(bake.times.size() == 61 && bake.poses.size() == 61, "inclusive 30 Hz samples");
            Near(bake.times.front(), 0, "first native time");
            Near(bake.times.back(), 2, "last native time");
            VectorNear(bake.poses[30].localTransforms[0].translation, {1, 2, 3});
            RotationNear(bake.poses[30].localTransforms[0].rotation, {0, 0, std::sqrt(0.5), std::sqrt(0.5)});
            VectorNear(bake.poses.back().localTransforms[0].translation, {2, 4, 6});
            for (const auto& pose : bake.poses)
            {
                Require(pose.localTransforms.size() == 3, "all joints are uploaded");
                VectorNear(pose.localTransforms[1].translation, skeleton.bones[1].bindPose.translation);
                VectorNear(pose.localTransforms[2].translation, skeleton.bones[2].bindPose.translation);
            }
        });

        add("bake.includes_off_grid_authored_times", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys.insert(clip.tracks[0].keys.begin() + 1, {0.123456, {{9, 0, 0}, {}}});
            const auto bake = Must(SM::BakeMotion(clip, TestSkeleton(), 30));
            Require(std::find(bake.times.begin(), bake.times.end(), static_cast<float>(0.123456)) != bake.times.end(),
                "authored time is retained in native precision");
            for (std::size_t i = 1; i < bake.times.size(); ++i)
            {
                Require(bake.times[i] > bake.times[i - 1], "native times strictly increase");
            }
        });

        add("bake.final_pose_uses_exact_source_endpoint", []
        {
            auto clip = TestClip();
            clip.duration = 1.00000001; // rounds down in native float
            clip.tracks[0].keys.back().time = clip.duration;
            clip.events.clear();
            const auto bake = Must(SM::BakeMotion(clip, TestSkeleton(), 30));
            VectorNear(bake.poses.back().localTransforms[0].translation, {2, 4, 6});
            RotationNear(bake.poses.back().localTransforms[0].rotation, {0, 0, 1, 0});
        });

        add("bake.bind_only_and_single_key_clips", []
        {
            auto clip = TestClip();
            clip.tracks.clear();
            auto bake = Must(SM::BakeMotion(clip, TestSkeleton()));
            VectorNear(bake.poses.back().localTransforms[2].translation, {1, 0, 0});
            clip.tracks.push_back({0, {{0.7, {{4, 5, 6}, {}}, SM::Interpolation::Step}}});
            bake = Must(SM::BakeMotion(clip, TestSkeleton()));
            VectorNear(bake.poses.front().localTransforms[0].translation, {4, 5, 6});
            VectorNear(bake.poses.back().localTransforms[0].translation, {4, 5, 6});
        });

        add("bake.smoothstep_native_linear_error_bound", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys[0].interpolation = SM::Interpolation::SmoothStep;
            const auto bake = Must(SM::BakeMotion(clip, TestSkeleton(), 120));
            // Independent analytic translation and linear reconstruction between native samples.
            for (std::size_t i = 1; i < bake.times.size(); ++i)
            {
                const double time = (bake.times[i - 1] + static_cast<double>(bake.times[i])) * 0.5;
                const double u = time / 2.0;
                const double expectedX = 2.0 * u * u * (3.0 - 2.0 * u);
                const double nativeX = (bake.poses[i - 1].localTransforms[0].translation.x
                    + bake.poses[i].localTransforms[0].translation.x) * 0.5;
                Near(nativeX, expectedX, "120 Hz smoothstep translation error", 0.00003);
            }
        });

        add("bake.rejects_bad_rates_and_duration", []
        {
            for (float rate : {0.0f, 29.0f, 241.0f, std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity()})
            {
                Reject(SM::BakeMotion(TestClip(), TestSkeleton(), rate));
            }
            auto clip = TestClip();
            clip.duration = 601;
            Reject(SM::BakeMotion(clip, TestSkeleton()));
        });

        add("bake.rejects_corrupt_skeleton_and_clip", []
        {
            Reject(SM::BakeMotion(TestClip(), {}));
            auto clip = TestClip();
            clip.tracks[0].boneIndex = 1000;
            Reject(SM::BakeMotion(clip, TestSkeleton()));
            auto skeleton = TestSkeleton();
            skeleton.bones[0].parent = 2;
            Reject(SM::BakeMotion(TestClip(), skeleton));
        });

        add("bake.rejects_discontinuous_segments", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys[0].interpolation = SM::Interpolation::Step;
            const auto bake = SM::BakeMotion(clip, TestSkeleton());
            Reject(bake);
            Require(bake.value.times.empty() && bake.value.poses.empty(), "no partial bake on failure");
        });

        add("bake.rejects_float_key_time_collision", []
        {
            auto clip = TestClip();
            clip.tracks[0].keys = {{1.0, {}}, {1.000000001, {{2, 0, 0}, {}}}};
            Require(SM::ValidateClip(clip, TestSkeleton()).empty(), "valid in double precision");
            Reject(SM::BakeMotion(clip, TestSkeleton()));
        });

        add("bake.rejects_transform_budget_before_pose_allocation", []
        {
            auto skeleton = TestSkeleton();
            for (int i = 3; i < 100; ++i)
            {
                skeleton.bones.push_back({"bone_" + std::to_string(i), 0, {}});
            }
            auto clip = TestClip();
            clip.duration = 600;
            const auto bake = SM::BakeMotion(clip, skeleton, 240);
            Reject(bake);
            Require(bake.value.poses.empty(), "budget failure is atomic");
            Require(bake.error.find("transform") != std::string::npos, "reports transform budget");
        });

        add("bake.rejects_validation_work_budget", []
        {
            auto clip = TestClip();
            clip.duration = 600;
            clip.tracks[0].keys.clear();
            for (int i = 0; i < 1000; ++i)
            {
                clip.tracks[0].keys.push_back({i * 0.6, {}});
            }
            const auto bake = SM::BakeMotion(clip, TestSkeleton(), 120);
            Reject(bake);
            Require(bake.error.find("work budget") != std::string::npos, "reports validation-work budget");
        });

        add("bake.counts_deep_hierarchy_validation_work", []
        {
            auto skeleton = TestSkeleton();
            for (int i = 3; i < 1000; ++i)
            {
                skeleton.bones.push_back({"bone_" + std::to_string(i), i - 1, {}});
            }
            const auto bake = SM::BakeMotion(TestClip(), skeleton, 120);
            Reject(bake);
            Require(bake.error.find("work budget") != std::string::npos, "deep ancestor walks count against the budget");
        });

        return tests;
    }
}

int main(int argc, char** argv)
{
    // CI and command-line runs must report CRT failures instead of opening a modal dialog.
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    const auto tests = Tests();
    const std::string_view filter = argc > 1 ? argv[1] : "";
    if (filter == "--list")
    {
        for (const auto& [name, body] : tests)
        {
            static_cast<void>(body);
            std::cout << name << '\n';
        }
        return 0;
    }
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const auto& [name, body] : tests)
    {
        if (!filter.empty() && name.find(filter) == std::string::npos)
        {
            continue;
        }
        try
        {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
        catch (...)
        {
            ++failed;
            std::cerr << "FAIL " << name << ": non-standard exception\n";
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    if (passed + failed == 0)
    {
        std::cerr << "No tests matched the filter\n";
        return 2;
    }
    return failed == 0 ? 0 : 1;
}
