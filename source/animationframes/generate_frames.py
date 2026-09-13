#!/usr/bin/env python3
#
# Copyright 2026 Aarav Ravindra Kharade
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Generate boot animation frames: ONLY the nucleus expanding.
The orbiting electrons are handled by Swift after the kernel boots.
Output: animation.bin (concatenated raw 32-bit BGRA frames, 200x200 each)
"""
import math
import os
import struct
import sys

FRAME_COUNT = 60
WIDTH = 200
HEIGHT = 200
CX = WIDTH / 2.0
CY = HEIGHT / 2.0
NUCLEUS_MAX_R = 28.0


def main():
    if len(sys.argv) < 2:
        print("Usage: generate_frames.py <output-dir>")
        sys.exit(1)

    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "animation.bin")

    all_frames = bytearray()

    for f in range(FRAME_COUNT):
        # Ease-out cubic expansion
        p = f / float(FRAME_COUNT - 1)
        p = 1.0 - (1.0 - p) ** 3
        radius = p * NUCLEUS_MAX_R

        frame = bytearray()
        for y in range(HEIGHT):
            for x in range(WIDTH):
                dx = x - CX
                dy = y - CY
                dist = math.sqrt(dx * dx + dy * dy)

                if dist < radius:
                    val = 255
                elif dist < radius + 1.5:
                    val = int(255 * (1.0 - (dist - radius) / 1.5))
                else:
                    val = 0

                # 32-bit BGRA
                frame.extend(struct.pack('BBBB', val, val, val, 0xFF))

        all_frames.extend(frame)

    with open(out_path, 'wb') as fp:
        fp.write(all_frames)

    size_kb = len(all_frames) // 1024
    print(f"  Generated {FRAME_COUNT} frames ({WIDTH}x{HEIGHT}x32bpp) "
          f"-> animation.bin ({size_kb} KB)")


if __name__ == '__main__':
    main()
