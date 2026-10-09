#!/usr/bin/env python3
# Copyright (c) 2026 Wanted Engine contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Generate the original rigid-skinned courier mannequin with no external assets.

Run with Python 3.10+. Output is a self-contained glTF 2.0 file, in meters,
using the existing seven-joint ScriptMotion test rig. No Blender dependency.
"""
import argparse
import base64
import copy
import json
import struct
from pathlib import Path


def generate(skeleton):
    document = {
        "asset": {"version": "2.0", "generator": "Outlaw Engine original courier generator",
                  "copyright": "2026 Wanted Engine contributors; Apache-2.0 OR MIT"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [], "skins": [], "meshes": [], "materials": [],
        "accessors": [], "bufferViews": [], "buffers": [],
    }
    binary = bytearray()

    def accessor(values, component, shape, target=None, bounds=False):
        formats = {5126: "f", 5123: "H"}
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[shape]
        while len(binary) % 4:
            binary.append(0)
        start = len(binary)
        flat = [c for row in values for c in row]
        binary.extend(struct.pack("<" + formats[component] * len(flat), *flat))
        view = {"buffer": 0, "byteOffset": start, "byteLength": len(binary) - start}
        if target:
            view["target"] = target
        document["bufferViews"].append(view)
        item = {"bufferView": len(document["bufferViews"]) - 1, "componentType": component,
                "count": len(values), "type": shape}
        if bounds:
            item["min"] = [min(row[i] for row in values) for i in range(width)]
            item["max"] = [max(row[i] for row in values) for i in range(width)]
        document["accessors"].append(item)
        return len(document["accessors"]) - 1

    def y_up(p):
        return [p[0], p[2], -p[1]]

    bones = skeleton["bones"]
    names = {bone["name"]: i for i, bone in enumerate(bones)}
    world = []
    for bone in bones:
        assert bone.get("rotation", [0, 0, 0, 1]) == [0, 0, 0, 1], "generator expects identity bind rotations"
        node = {"name": bone["name"], "translation": y_up(bone.get("translation", [0, 0, 0]))}
        parent = names[bone["parent"]] if bone["parent"] else -1
        assert parent < len(world), "source rig must be parent-first"
        world.append([node["translation"][i] + (world[parent][i] if parent >= 0 else 0) for i in range(3)])
        document["nodes"].append(node)
        if parent >= 0:
            document["nodes"][parent].setdefault("children", []).append(len(world) - 1)
    inverse_binds = []
    for x, y, z in world:
        inverse_binds.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, -y, -z, 1])
    document["skins"] = [{"name": "courier_gltf", "joints": list(range(len(bones))), "skeleton": 0,
                          "inverseBindMatrices": accessor(inverse_binds, 5126, "MAT4")}]

    palette = {"coat": [0.12, 0.28, 0.27, 1], "shirt": [0.78, 0.7, 0.51, 1],
               "skin": [0.58, 0.36, 0.22, 1], "leather": [0.18, 0.085, 0.035, 1],
               "trousers": [0.2, 0.25, 0.3, 1], "eyes": [0.025, 0.022, 0.019, 1]}
    for name, color in palette.items():
        document["materials"].append({"name": name,
            "pbrMetallicRoughness": {"baseColorFactor": color, "metallicFactor": 0, "roughnessFactor": 0.85}})
    material_ids = {name: i for i, name in enumerate(palette)}
    primitives = []

    def box(center, size, joint_name, material):
        x, y, z = center
        a, b, c = [v / 2 for v in size]
        corners = [(x - a, y - b, z - c), (x + a, y - b, z - c),
                   (x + a, y + b, z - c), (x - a, y + b, z - c),
                   (x - a, y - b, z + c), (x + a, y - b, z + c),
                   (x + a, y + b, z + c), (x - a, y + b, z + c)]
        faces = [((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)),
                 ((0, 1, 5, 4), (0, -1, 0)), ((3, 7, 6, 2), (0, 1, 0)),
                 ((0, 4, 7, 3), (-1, 0, 0)), ((1, 2, 6, 5), (1, 0, 0))]
        positions, normals, uvs, indices = [], [], [], []
        for face, normal in faces:
            offset = len(positions)
            positions.extend(y_up(corners[i]) for i in face)
            normals.extend([y_up(normal)] * 4)
            uvs.extend([(0, 0), (1, 0), (1, 1), (0, 1)])
            indices.extend([[offset + i] for i in (0, 1, 2, 0, 2, 3)])
        count = len(positions)
        primitives.append({"attributes": {
            "POSITION": accessor(positions, 5126, "VEC3", 34962, True),
            "NORMAL": accessor(normals, 5126, "VEC3", 34962),
            "TEXCOORD_0": accessor(uvs, 5126, "VEC2", 34962),
            "JOINTS_0": accessor([[names[joint_name], 0, 0, 0]] * count, 5123, "VEC4", 34962),
            "WEIGHTS_0": accessor([[1, 0, 0, 0]] * count, 5126, "VEC4", 34962)},
            "indices": accessor(indices, 5123, "SCALAR", 34963), "material": material_ids[material]})

    box((0, 0, 1.22), (0.42, 0.26, 0.58), "spine", "coat")
    box((0, -0.141, 1.32), (0.13, 0.025, 0.36), "spine", "shirt")
    box((0, 0, 0.94), (0.43, 0.28, 0.10), "pelvis", "leather")
    for x in (-0.12, 0.12):
        side = "l" if x < 0 else "r"
        if "thigh_" + side in names:
            box((x, 0, 0.72), (0.17, 0.21, 0.36), "thigh_" + side, "trousers")
            box((x, 0, 0.35), (0.15, 0.20, 0.36), "shin_" + side, "trousers")
            box((x, -0.05, 0.12), (0.19, 0.34, 0.24), "foot_" + side, "leather")
        else:
            box((x, 0, 0.55), (0.17, 0.21, 0.7), "pelvis", "trousers")
            box((x, -0.05, 0.12), (0.19, 0.34, 0.24), "root", "leather")
    box((0, 0, 1.58), (0.12, 0.14, 0.18), "head", "skin")
    box((0, 0, 1.76), (0.26, 0.25, 0.30), "head", "skin")
    box((0, 0, 1.94), (0.54, 0.47, 0.035), "head", "leather")
    box((0, 0, 2.025), (0.31, 0.3, 0.15), "head", "leather")
    for x in (-0.065, 0.065):
        box((x, -0.131, 1.80), (0.045, 0.012, 0.03), "head", "eyes")
    if "upperarm_l" in names:
        box((-0.34, 0, 1.55), (0.28, 0.18, 0.18), "upperarm_l", "coat")
        box((-0.605, 0, 1.55), (0.25, 0.15, 0.15), "forearm_l", "shirt")
        box((-0.795, 0, 1.55), (0.13, 0.14, 0.12), "hand_l", "skin")
    else:
        box((-0.30, 0, 1.29), (0.14, 0.21, 0.52), "spine", "coat")
        box((-0.30, 0, 0.99), (0.13, 0.14, 0.14), "spine", "skin")
    box((0.34, 0, 1.55), (0.28, 0.18, 0.18), "upperarm_r", "coat")
    box((0.605, 0, 1.55), (0.25, 0.15, 0.15), "forearm_r", "shirt")
    box((0.795, 0, 1.55), (0.13, 0.14, 0.12), "hand_r", "skin")
    document["meshes"] = [{"name": "Courier", "primitives": primitives}]
    document["nodes"][0].setdefault("children", []).append(len(document["nodes"]))
    document["nodes"].append({"name": "CourierMesh", "mesh": 0, "skin": 0})
    document["buffers"] = [{"byteLength": len(binary),
        "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")}]
    return document


def motion_sources(skeleton, wave):
    """Convert the Z-up example's local channels to the glTF joints' Y-up basis.

    O3DE's AssImpReadRootTransform converts at the scene root. Child joint
    coordinates stay in their source basis, so the Z-up clip cannot be reused
    unchanged on this glTF actor. Native playback captures the imported root bind.
    """
    skeleton, wave = copy.deepcopy(skeleton), copy.deepcopy(wave)
    skeleton["name"] = wave["skeleton"] = "courier_gltf"
    wave["name"] = "courier_wave"
    wave["events"][0]["payload"] = {"speaker": "Ada Mercer", "subtitle": "Afternoon. I could use a hand."}
    transforms = skeleton["bones"] + [key for track in wave["tracks"] for key in track["keys"]]
    for transform in transforms:
        if "translation" in transform:
            x, y, z = transform["translation"]
            transform["translation"] = [x, z, -y]
        if "rotation" in transform:
            x, y, z, w = transform["rotation"]
            transform["rotation"] = [x, z, -y, w]
    return skeleton, wave


if __name__ == "__main__":
    examples = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--motion-dir", type=Path, help="Also write the matching Y-up local skeleton and wave JSON")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    skeleton = json.loads((examples / "wanted_test_skeleton.json").read_text())
    args.output.write_text(json.dumps(generate(skeleton), indent=2) + "\n", encoding="utf-8")
    if args.motion_dir:
        args.motion_dir.mkdir(parents=True, exist_ok=True)
        wave = json.loads((examples / "frontier_wave.scriptmotion.json").read_text())
        local_skeleton, local_wave = motion_sources(skeleton, wave)
        for filename, data in [("courier.skeleton.json", local_skeleton), ("courier_wave.scriptmotion.json", local_wave)]:
            (args.motion_dir / filename).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {args.output.name}: seven joints, eighteen rigid-skinned boxes, no external assets")
