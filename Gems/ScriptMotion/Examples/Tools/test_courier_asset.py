#!/usr/bin/env python3
# Copyright (c) 2026 Wanted Engine contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Structural/skinning checks for the original glTF. Does not replace O3DE import QA."""
import base64
import json
import math
import struct
import unittest
from pathlib import Path

from generate_courier import generate, motion_sources

REPO = Path(__file__).resolve().parents[4]
EXAMPLES = REPO / "Gems/ScriptMotion/Examples"


class CourierAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.skeleton = json.loads((EXAMPLES / "wanted_test_skeleton.json").read_text())
        cls.asset = json.loads((REPO / "Projects/Wanted/Assets/Characters/ScriptMotionCourier.gltf").read_text())
        cls.buffer = base64.b64decode(cls.asset["buffers"][0]["uri"].split(",", 1)[1], validate=True)

    def read_accessor(self, index):
        a = self.asset["accessors"][index]
        view = self.asset["bufferViews"][a["bufferView"]]
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
        fmt = {5126: "f", 5123: "H"}[a["componentType"]]
        stride = struct.calcsize("<" + fmt * width)
        offset = view["byteOffset"] + a.get("byteOffset", 0)
        return [struct.unpack_from("<" + fmt * width, self.buffer, offset + i * stride) for i in range(a["count"])]

    def test_asset_is_reproducible_and_self_contained(self):
        self.assertEqual(self.asset, generate(self.skeleton))
        self.assertEqual(len(self.buffer), self.asset["buffers"][0]["byteLength"])
        self.assertNotIn("images", self.asset)
        self.assertIn("Apache-2.0 OR MIT", self.asset["asset"]["copyright"])

    def test_skin_joints_and_inverse_bind_matrices(self):
        skin = self.asset["skins"][0]
        self.assertEqual([self.asset["nodes"][i]["name"] for i in skin["joints"]],
                         [b["name"] for b in self.skeleton["bones"]])
        parent_by_node = {}
        for parent, node in enumerate(self.asset["nodes"]):
            for child in node.get("children", []):
                self.assertNotIn(child, parent_by_node)
                parent_by_node[child] = parent
        inverse_binds = self.read_accessor(skin["inverseBindMatrices"])
        for joint, matrix in zip(skin["joints"], inverse_binds):
            position = [0, 0, 0]
            node = joint
            visited = set()
            while node is not None:
                self.assertNotIn(node, visited)
                visited.add(node)
                position = [a + b for a, b in zip(position, self.asset["nodes"][node].get("translation", [0, 0, 0]))]
                node = parent_by_node.get(node)
            for i in range(3):
                self.assertAlmostEqual(matrix[12 + i] + position[i], 0, places=6)

    def test_all_vertices_have_finite_normalized_weights_and_valid_joints(self):
        for primitive in self.asset["meshes"][0]["primitives"]:
            attrs = primitive["attributes"]
            weights, joints = self.read_accessor(attrs["WEIGHTS_0"]), self.read_accessor(attrs["JOINTS_0"])
            self.assertEqual(len(weights), len(self.read_accessor(attrs["POSITION"])))
            for weight, joint in zip(weights, joints):
                self.assertTrue(all(math.isfinite(w) and w >= 0 for w in weight))
                self.assertAlmostEqual(sum(weight), 1)
                self.assertTrue(all(0 <= j < len(self.skeleton['bones']) for j in joint))

    def test_triangles_have_outward_normals_and_valid_buffer_ranges(self):
        for view in self.asset["bufferViews"]:
            self.assertEqual(view["byteOffset"] % 4, 0)
            self.assertLessEqual(view["byteOffset"] + view["byteLength"], len(self.buffer))
        for primitive in self.asset["meshes"][0]["primitives"]:
            positions = self.read_accessor(primitive["attributes"]["POSITION"])
            normals = self.read_accessor(primitive["attributes"]["NORMAL"])
            indices = [i[0] for i in self.read_accessor(primitive["indices"])]
            self.assertEqual(len(indices) % 3, 0)
            for n in range(0, len(indices), 3):
                i, j, k = indices[n:n + 3]
                self.assertLess(max(i, j, k), len(positions))
                u = [b - a for a, b in zip(positions[i], positions[j])]
                v = [b - a for a, b in zip(positions[i], positions[k])]
                cross = [u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2], u[0]*v[1] - u[1]*v[0]]
                self.assertGreater(sum(a*b for a, b in zip(cross, normals[i])), 0)

    def test_sample_clip_uses_existing_joints_and_ada_dialogue(self):
        clip = json.loads((REPO / "Projects/Wanted/Assets/ScriptMotion/courier_wave.scriptmotion.json").read_text())
        self.assertEqual(clip["skeleton"], "courier_gltf")
        self.assertTrue(set(t["bone"] for t in clip["tracks"]) <= set(b["name"] for b in self.skeleton["bones"]))
        self.assertEqual(clip["events"][0]["payload"]["speaker"], "Ada Mercer")

    def test_motion_sources_use_gltf_joint_local_basis(self):
        wave = json.loads((EXAMPLES / "frontier_wave.scriptmotion.json").read_text())
        rig, clip = motion_sources(self.skeleton, wave)
        local = REPO / "Projects/Wanted/Assets/ScriptMotion"
        self.assertEqual(rig, json.loads((local / "courier.skeleton.json").read_text()))
        self.assertEqual(clip, json.loads((local / "courier_wave.scriptmotion.json").read_text()))
        self.assertEqual(rig["bones"][1]["translation"], [0, 0.95, 0])
        arm = next(track for track in clip["tracks"] if track["bone"] == "upperarm_r")
        # Raising an X-axis arm toward glTF +Y requires rotation around +Z.
        self.assertAlmostEqual(arm["keys"][1]["rotation"][2], 0.5)
        self.assertAlmostEqual(arm["keys"][1]["rotation"][1], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
