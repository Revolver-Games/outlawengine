# Original ScriptMotion courier

`ScriptMotionCourier.gltf` is an original block mannequin created for this
repository. It is a visible **rigged source asset**, not a completed Ada Mercer
character or a processed O3DE Actor. It contains 432 vertices, 216 triangles,
seven joints, normalized rigid skin weights, six plain materials, and an embedded
buffer. It has no external textures, downloads, or third-party character assets.

Copyright (c) 2026 Wanted Engine contributors. Distributed under the repository's
Apache-2.0 OR MIT license (see `LICENSE_APACHE2.TXT` and `LICENSE_MIT.TXT` at the
repository root). The mesh, materials, rig, generator and sample motion are
original; no Rockstar/Red Dead assets or other game assets are included.

The seven joint names are shared with `Gems/ScriptMotion/Examples`, but glTF uses
Y-up local coordinates. The matching source skeleton and animation are in
`Assets/ScriptMotion/courier.skeleton.json` and `courier_wave.scriptmotion.json`.
The clip raises the right arm and includes an Ada Mercer greeting event. There
is no subtitle UI or voice recording in this asset.

Reproduce and check from the repository root:

```sh
python Gems/ScriptMotion/Examples/Tools/generate_courier.py --output Projects/Wanted/Assets/Characters/ScriptMotionCourier.gltf --motion-dir Projects/Wanted/Assets/ScriptMotion
python Gems/ScriptMotion/Examples/Tools/test_courier_asset.py
```

The generator uses only Python's standard library. Regeneration replaces these
generated files; save any deliberate manual variants under another name.
The sample was imported, skinned, and posed from the C++ evaluator in Blender
5.2.1 LTS. **O3DE Asset Processor import and editor/runtime rendering are pending.**
Follow `Gems/ScriptMotion/ACTOR_PREVIEW.md` for the engine verification procedure.
