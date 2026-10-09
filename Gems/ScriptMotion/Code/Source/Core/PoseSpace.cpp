/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/ScriptMotion.h>

#include <cmath>
#include <string>
#include <vector>

namespace Wanted::ScriptMotion
{
    namespace
    {
        constexpr double MaxModelCoordinate = 1e9;

        double QuaternionNorm(Quaternion q)
        {
            return std::hypot(std::hypot(q.x, q.y), std::hypot(q.z, q.w));
        }

        Quaternion Unit(Quaternion q)
        {
            const double length = QuaternionNorm(q);
            return {q.x / length, q.y / length, q.z / length, q.w / length};
        }

        bool ValidLocal(LocalTransform transform)
        {
            const Vec3 v = transform.translation;
            const double norm = QuaternionNorm(transform.rotation);
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
                && std::abs(v.x) <= MaxModelCoordinate && std::abs(v.y) <= MaxModelCoordinate
                && std::abs(v.z) <= MaxModelCoordinate && std::isfinite(norm) && norm >= 1e-12;
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

        Vec3 Cross(Vec3 a, Vec3 b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }

        Vec3 Rotate(Quaternion unitRotation, Vec3 v)
        {
            // q * [v, 0] * conjugate(q), using a unit quaternion.
            const Vec3 axis{unitRotation.x, unitRotation.y, unitRotation.z};
            const Vec3 t = Cross(axis, v);
            const Vec3 twice{2 * t.x, 2 * t.y, 2 * t.z};
            const Vec3 correction = Cross(axis, twice);
            return {v.x + unitRotation.w * twice.x + correction.x,
                v.y + unitRotation.w * twice.y + correction.y,
                v.z + unitRotation.w * twice.z + correction.z};
        }

        bool InRange(Vec3 v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
                && std::abs(v.x) <= MaxModelCoordinate && std::abs(v.y) <= MaxModelCoordinate
                && std::abs(v.z) <= MaxModelCoordinate;
        }
    }

    Result<Pose> ComputeModelSpacePose(const Skeleton& skeleton, const Pose& localPose)
    {
        if (const std::string error = ValidateSkeleton(skeleton); !error.empty())
        {
            return {{}, error};
        }
        const std::size_t count = skeleton.bones.size();
        if (localPose.localTransforms.size() != count)
        {
            return {{}, "Pose bone count does not match skeleton"};
        }
        for (const LocalTransform& local : localPose.localTransforms)
        {
            if (!ValidLocal(local))
            {
                return {{}, "Model-space evaluation received an invalid local transform"};
            }
        }

        Pose result;
        result.localTransforms.resize(count);
        std::vector<bool> resolved(count, false);
        std::vector<std::size_t> pending;
        pending.reserve(count);

        // Supports valid skeletons whose parents appear after their children.
        // ValidateSkeleton has already checked every parent chain for cycles.
        for (std::size_t index = 0; index < count; ++index)
        {
            pending.clear();
            std::size_t current = index;
            while (!resolved[current])
            {
                pending.push_back(current);
                const int parent = skeleton.bones[current].parent;
                if (parent == -1) { break; }
                current = static_cast<std::size_t>(parent);
            }
            for (auto it = pending.rbegin(); it != pending.rend(); ++it)
            {
                const std::size_t boneIndex = *it;
                LocalTransform model = localPose.localTransforms[boneIndex];
                model.rotation = Unit(model.rotation);
                const int parentIndex = skeleton.bones[boneIndex].parent;
                if (parentIndex >= 0)
                {
                    const LocalTransform& parent = result.localTransforms[static_cast<std::size_t>(parentIndex)];
                    const Vec3 offset = Rotate(parent.rotation, model.translation);
                    model.translation = {parent.translation.x + offset.x,
                        parent.translation.y + offset.y, parent.translation.z + offset.z};
                    model.rotation = Unit(Multiply(parent.rotation, model.rotation));
                }
                if (!InRange(model.translation) || !std::isfinite(QuaternionNorm(model.rotation)))
                {
                    return {{}, "Model-space transform exceeds supported coordinate or rotation limits"};
                }
                result.localTransforms[boneIndex] = model;
                resolved[boneIndex] = true;
            }
        }
        return {std::move(result), {}};
    }

    Result<Pose> EvaluateModelSpacePose(const Clip& clip, const Skeleton& skeleton,
        double timelineSeconds, PlaybackOptions options)
    {
        Result<Pose> local = EvaluatePose(clip, skeleton, timelineSeconds, options);
        if (!local) { return {{}, std::move(local.error)}; }
        return ComputeModelSpacePose(skeleton, local.value);
    }
}
