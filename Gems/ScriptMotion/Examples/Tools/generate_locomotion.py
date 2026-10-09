#!/usr/bin/env python3
"""Original 16-joint courier and three text clips. No external art or packages.
Copyright (c) 2026 Revolver Games contributors.
SPDX-License-Identifier: Apache-2.0 OR MIT
"""
import json
import math
from pathlib import Path
from generate_courier import generate, motion_sources


def sources():
    examples = Path(__file__).resolve().parents[1]
    rig = json.loads((examples / 'wanted_test_skeleton.json').read_text())
    wave = json.loads((examples / 'frontier_wave.scriptmotion.json').read_text())
    for name, parent, translation in [
        ('upperarm_l', 'spine', [-0.2, 0, 0.25]),
        ('forearm_l', 'upperarm_l', [-0.28, 0, 0]),
        ('hand_l', 'forearm_l', [-0.25, 0, 0]),
        ('thigh_l', 'pelvis', [-0.12, 0, -0.05]),
        ('shin_l', 'thigh_l', [0, 0, -0.4]),
        ('foot_l', 'shin_l', [0, 0, -0.4]),
        ('thigh_r', 'pelvis', [0.12, 0, -0.05]),
        ('shin_r', 'thigh_r', [0, 0, -0.4]),
        ('foot_r', 'shin_r', [0, 0, -0.4]),
    ]:
        rig['bones'].append({'name': name, 'parent': parent, 'translation': translation})
    mesh = generate(rig)
    mesh['skins'][0]['name'] = 'courier_locomotion'
    rig, wave = motion_sources(rig, wave)
    rig['name'] = wave['skeleton'] = 'courier_locomotion'
    wave['name'] = 'courier_greeting'
    bones = {b['name']: b for b in rig['bones']}

    def rotation(axis, angle):
        half = math.radians(angle) / 2
        return [math.sin(half) * c for c in axis] + [math.cos(half)]

    def track(name, keys, axis=(1, 0, 0)):
        return {'bone': name, 'keys': [
            {'time': t, 'translation': bones[name]['translation'],
             'rotation': rotation(axis, angle), 'interpolation': 'smoothstep'} for t, angle in keys]}

    def clip(name, duration, tracks, events=None):
        return {'formatVersion': 1, 'name': name, 'skeleton': rig['name'],
                'duration': duration, 'tracks': tracks, 'events': events or []}

    idle = clip('courier_idle_breathing', 4, [
        track('spine', [(0, 0), (2, 1.5), (4, 0)]),
        track('head', [(0, 0), (2, -1.0), (4, 0)]),
        track('upperarm_r', [(0, -75), (2, -73), (4, -75)], (0, 0, 1)),
        track('upperarm_l', [(0, 75), (2, 73), (4, 75)], (0, 0, 1)),
    ])
    walking = clip('courier_walk', 1, [
        track('thigh_l', [(0, -25), (.5, 25), (1, -25)]),
        track('thigh_r', [(0, 25), (.5, -25), (1, 25)]),
        track('shin_l', [(0, 5), (.25, 45), (.5, 5), (1, 5)]),
        track('shin_r', [(0, 5), (.5, 5), (.75, 45), (1, 5)]),
        track('foot_l', [(0, -5), (.25, -25), (.5, -5), (1, -5)]),
        track('foot_r', [(0, -5), (.5, -5), (.75, -25), (1, -5)]),
        track('upperarm_r', [(0, -75), (1, -75)], (0, 0, 1)),
        track('upperarm_l', [(0, 75), (1, 75)], (0, 0, 1)),
        {'bone': 'root', 'keys': [
            {'time': 0, 'translation': [0, 0, 0]},
            {'time': 1, 'translation': [0, 0, 1.2]}]},
    ], [{'time': .25, 'name': 'footstep', 'payload': {'foot': 'left'}},
        {'time': .75, 'name': 'footstep', 'payload': {'foot': 'right'}}])
    # The right arm is authored by the original wave; keep the other arm relaxed.
    wave['tracks'].append(track('upperarm_l', [(0, 75), (wave['duration'], 75)], (0, 0, 1)))
    return mesh, rig, idle, walking, wave


def main():
    root = Path(__file__).resolve().parents[4]
    assets = root / 'Projects/Wanted/Assets'
    mesh, rig, idle, walk, wave = sources()
    for path, value in [(assets / 'Characters/ScriptMotionLocomotionCourier.gltf', mesh),
                        (assets / 'ScriptMotion/locomotion.skeleton.json', rig),
                        (assets / 'ScriptMotion/idle.scriptmotion.json', idle),
                        (assets / 'ScriptMotion/walk.scriptmotion.json', walk),
                        (assets / 'ScriptMotion/greeting.scriptmotion.json', wave)]:
        path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
