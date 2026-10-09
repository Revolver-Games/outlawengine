#!/usr/bin/env python3
# Copyright (c) 2026 Revolver Games contributors.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import base64
import json
import unittest
from generate_locomotion import sources
import test_courier_asset as courier
from test_courier_asset import REPO


class LocomotionAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.expected = sources()
        cls.asset = json.loads((REPO / 'Projects/Wanted/Assets/Characters/ScriptMotionLocomotionCourier.gltf').read_text())
        cls.skeleton = cls.expected[1]
        cls.buffer = base64.b64decode(cls.asset['buffers'][0]['uri'].split(',', 1)[1], validate=True)

    read_accessor = courier.CourierAssetTests.read_accessor
    test_skin = courier.CourierAssetTests.test_skin_joints_and_inverse_bind_matrices
    test_weights = courier.CourierAssetTests.test_all_vertices_have_finite_normalized_weights_and_valid_joints
    test_geometry = courier.CourierAssetTests.test_triangles_have_outward_normals_and_valid_buffer_ranges

    def test_reproducible_sources(self):
        self.assertEqual(self.asset, self.expected[0])
        self.assertEqual(len(self.skeleton['bones']), 16)
        for name, value in zip(['locomotion.skeleton', 'idle.scriptmotion', 'walk.scriptmotion', 'greeting.scriptmotion'], self.expected[1:]):
            self.assertEqual(json.loads((REPO / f'Projects/Wanted/Assets/ScriptMotion/{name}.json').read_text()), value)

    def test_loop_seams_and_independent_leg_skinning(self):
        for clip in self.expected[2:4]:
            for track in clip['tracks']:
                a, b = track['keys'][0], track['keys'][-1]
                if track['bone'] != 'root':
                    self.assertEqual(a['rotation'], b['rotation'])
                    self.assertEqual(a['translation'], b['translation'])
        used = set()
        for primitive in self.asset['meshes'][0]['primitives']:
            used.update(row[0] for row in self.read_accessor(primitive['attributes']['JOINTS_0']))
        names = {self.skeleton['bones'][index]['name'] for index in used}
        self.assertTrue({'thigh_l', 'shin_l', 'foot_l', 'thigh_r', 'shin_r', 'foot_r'} <= names)


if __name__ == '__main__':
    unittest.main(verbosity=2)
