# Model-space poses (ScriptMotion)

ScriptMotion stores each joint's translation and rotation in its **parent's local space**.
`ComputeModelSpacePose(skeleton, localPose)` walks the validated hierarchy and returns
bone transforms expressed in the skeleton's model space. `EvaluateModelSpacePose`
first samples a validated clip using the existing evaluator, then composes its pose.

For a child bone:

- model rotation = parent model rotation * child local rotation;
- model translation = parent model translation + rotated child local translation.

The API supports multiple roots and valid out-of-order bone declarations. It rejects
invalid skeletons, pose-size mismatches, invalid transforms, and accumulated positions
outside the portable core's coordinate limits. Returned values retain the original
bone order. No inverse bind matrices, skinning palette, global entity transform,
IK solve, or O3DE Actor retargeting are implemented by this change.

## Standalone verification

From the engine repository root:

```sh
cmake -S Gems/ScriptMotion/Standalone -B build/scriptmotion -DCMAKE_BUILD_TYPE=Debug
cmake --build build/scriptmotion --parallel 2
ctest --test-dir build/scriptmotion --output-on-failure
```

Added four standalone test cases under `model_space.`:
parent rotation/translation, unordered parents, malformed input, and clip integration.
These changes have **not yet been compiled or executed in this GitHub-only session**.
They require a local compiler/runner or CI build; a complete O3DE build and visual
Actor test remain separate integration gates.

These utilities prepare attachment sockets, projected collision targets and
skeleton-space debugging; do not treat them as completed engine attachment, physics,
or character rendering systems.
