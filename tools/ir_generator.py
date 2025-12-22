#!/usr/bin/env python3
"""
IR Scene Generator for VGCPU-Benchmark
Blueprint Reference: Chapter 5, §5.3 — IR Binary Layout

Generates binary .irbin scene files for benchmark testing.
"""

import struct
import zlib
import os
from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Tuple
import json
import math

# IR Format Constants
IR_MAGIC = b'VGIR'
IR_MAJOR_VERSION = 1
IR_MINOR_VERSION = 0

# Section Types
class SectionType(IntEnum):
    INFO = 0x01
    PAINT = 0x02
    PATH = 0x03
    COMMAND = 0x04
    EXTENSION = 0xFF

# Opcodes
class Opcode(IntEnum):
    END = 0x00
    SAVE = 0x01
    RESTORE = 0x02
    CLEAR = 0x10
    SET_MATRIX = 0x20
    CONCAT_MATRIX = 0x21
    SET_FILL = 0x30
    SET_STROKE = 0x31
    FILL_PATH = 0x40
    STROKE_PATH = 0x41

# Path Verbs
class PathVerb(IntEnum):
    MOVE_TO = 0
    LINE_TO = 1
    QUAD_TO = 2
    CUBIC_TO = 3
    CLOSE = 4

# Paint Types
class PaintType(IntEnum):
    SOLID = 0
    LINEAR = 1
    RADIAL = 2

# Fill Rules
class FillRule(IntEnum):
    NON_ZERO = 0
    EVEN_ODD = 1

# Stroke Options
class StrokeCap(IntEnum):
    BUTT = 0
    ROUND = 1
    SQUARE = 2

class StrokeJoin(IntEnum):
    MITER = 0
    ROUND = 1
    BEVEL = 2

@dataclass
class GradientStop:
    offset: float
    color: int

@dataclass
class Paint:
    """Paint definition (solid color or gradient)."""
    paint_type: PaintType = PaintType.SOLID
    
    # Solid
    color: int = 0xFF000000  # RGBA8
    
    # Linear
    linear_stops: List[GradientStop] = field(default_factory=list)
    x0: float = 0.0
    y0: float = 0.0
    x1: float = 0.0
    y1: float = 0.0
    
    # Radial
    radial_stops: List[GradientStop] = field(default_factory=list)
    cx: float = 0.0
    cy: float = 0.0
    r: float = 0.0
    
    @staticmethod
    def solid(r: int, g: int, b: int, a: int = 255) -> 'Paint':
        color = r | (g << 8) | (b << 16) | (a << 24)
        return Paint(paint_type=PaintType.SOLID, color=color)
    
    @staticmethod
    def linear(x0: float, y0: float, x1: float, y1: float, stops: List[GradientStop]) -> 'Paint':
        return Paint(paint_type=PaintType.LINEAR, x0=x0, y0=y0, x1=x1, y1=y1, linear_stops=stops)

    @staticmethod
    def radial(cx: float, cy: float, r: float, stops: List[GradientStop]) -> 'Paint':
        return Paint(paint_type=PaintType.RADIAL, cx=cx, cy=cy, r=r, radial_stops=stops)

def rgba(r, g, b, a=255) -> int:
    return r | (g << 8) | (b << 16) | (a << 24)

@dataclass
class Path:
    """Path geometry definition."""
    verbs: List[PathVerb] = field(default_factory=list)
    points: List[float] = field(default_factory=list)
    
    def move_to(self, x: float, y: float) -> 'Path':
        self.verbs.append(PathVerb.MOVE_TO)
        self.points.extend([x, y])
        return self
    
    def line_to(self, x: float, y: float) -> 'Path':
        self.verbs.append(PathVerb.LINE_TO)
        self.points.extend([x, y])
        return self
    
    def quad_to(self, cx: float, cy: float, x: float, y: float) -> 'Path':
        self.verbs.append(PathVerb.QUAD_TO)
        self.points.extend([cx, cy, x, y])
        return self
    
    def cubic_to(self, c1x: float, c1y: float, c2x: float, c2y: float, x: float, y: float) -> 'Path':
        self.verbs.append(PathVerb.CUBIC_TO)
        self.points.extend([c1x, c1y, c2x, c2y, x, y])
        return self
    
    def close(self) -> 'Path':
        self.verbs.append(PathVerb.CLOSE)
        return self
    
    def rect(self, x: float, y: float, w: float, h: float) -> 'Path':
        """Add a rectangle path."""
        return self.move_to(x, y).line_to(x+w, y).line_to(x+w, y+h).line_to(x, y+h).close()
    
    def circle(self, cx: float, cy: float, r: float) -> 'Path':
        """Add a circle approximated with cubic beziers."""
        k = 0.5522847498
        self.move_to(cx + r, cy)
        self.cubic_to(cx + r, cy + r*k, cx + r*k, cy + r, cx, cy + r)
        self.cubic_to(cx - r*k, cy + r, cx - r, cy + r*k, cx - r, cy)
        self.cubic_to(cx - r, cy - r*k, cx - r*k, cy - r, cx, cy - r)
        self.cubic_to(cx + r*k, cy - r, cx + r, cy - r*k, cx + r, cy)
        return self.close()
    
    def spiral(self, cx: float, cy: float, start_r: float, end_r: float, turns: float):
        steps = 100
        for i in range(steps + 1):
            t = i / steps
            angle = t * turns * math.pi * 2
            r = start_r + (end_r - start_r) * t
            x = cx + math.cos(angle) * r
            y = cy + math.sin(angle) * r
            if i == 0:
                self.move_to(x, y)
            else:
                self.line_to(x, y)
        return self

@dataclass
class Command:
    """IR command."""
    opcode: Opcode
    args: bytes = b''

class IrBuilder:
    """Builder for IR binary files."""
    
    def __init__(self, width: int = 800, height: int = 600):
        self.width = width
        self.height = height
        self.paints: List[Paint] = []
        self.paths: List[Path] = []
        self.commands: List[Command] = []
        self.info: dict = {}
    
    def add_paint(self, paint: Paint) -> int:
        idx = len(self.paints)
        self.paints.append(paint)
        return idx
    
    def add_path(self, path: Path) -> int:
        idx = len(self.paths)
        self.paths.append(path)
        return idx
    
    def clear(self, r: int, g: int, b: int, a: int = 255):
        val = r | (g << 8) | (b << 16) | (a << 24)
        self.commands.append(Command(Opcode.CLEAR, struct.pack('<I', val)))
        return self
    
    def set_fill(self, paint_id: int, fill_rule: FillRule = FillRule.NON_ZERO):
        args = struct.pack('<HB', paint_id, fill_rule)
        self.commands.append(Command(Opcode.SET_FILL, args))
        return self
    
    def fill_path(self, path_id: int):
        args = struct.pack('<H', path_id)
        self.commands.append(Command(Opcode.FILL_PATH, args))
        return self
    
    def set_stroke(self, paint_id: int, width: float, cap: StrokeCap = StrokeCap.BUTT, join: StrokeJoin = StrokeJoin.MITER):
        opts = int(cap) | (int(join) << 2)
        args = struct.pack('<HfB', paint_id, width, opts)
        self.commands.append(Command(Opcode.SET_STROKE, args))
        return self
    
    def stroke_path(self, path_id: int):
        args = struct.pack('<H', path_id)
        self.commands.append(Command(Opcode.STROKE_PATH, args))
        return self
    
    def save(self):
        self.commands.append(Command(Opcode.SAVE))
        return self
    
    def restore(self):
        self.commands.append(Command(Opcode.RESTORE))
        return self
    
    def _build_paint_section(self) -> bytes:
        data = struct.pack('<H', len(self.paints))
        for paint in self.paints:
            data += struct.pack('<B', paint.paint_type)
            if paint.paint_type == PaintType.SOLID:
                data += struct.pack('<I', paint.color)
            elif paint.paint_type == PaintType.LINEAR:
                data += struct.pack('<ffffH', paint.x0, paint.y0, paint.x1, paint.y1, len(paint.linear_stops))
                for stop in paint.linear_stops:
                    data += struct.pack('<fI', stop.offset, stop.color)
            elif paint.paint_type == PaintType.RADIAL:
                data += struct.pack('<fffH', paint.cx, paint.cy, paint.r, len(paint.radial_stops))
                for stop in paint.radial_stops:
                    data += struct.pack('<fI', stop.offset, stop.color)
        return data
    
    def _build_path_section(self) -> bytes:
        data = struct.pack('<H', len(self.paths))
        for path in self.paths:
            data += struct.pack('<HH', len(path.verbs), len(path.points))
            for verb in path.verbs:
                data += struct.pack('<B', verb)
            for pt in path.points:
                data += struct.pack('<f', pt)
        return data
    
    def _build_command_section(self) -> bytes:
        data = b''
        for cmd in self.commands:
            data += struct.pack('<B', cmd.opcode) + cmd.args
        data += struct.pack('<B', Opcode.END)
        return data
    
    def _build_section(self, section_type: SectionType, payload: bytes) -> bytes:
        header_size = 6
        length = header_size + len(payload)
        header = struct.pack('<BBL', section_type, 0, length)
        return header + payload
    
    def build(self) -> bytes:
        sections = b''
        if self.paints: sections += self._build_section(SectionType.PAINT, self._build_paint_section())
        if self.paths: sections += self._build_section(SectionType.PATH, self._build_path_section())
        if self.commands: sections += self._build_section(SectionType.COMMAND, self._build_command_section())
        
        crc = zlib.crc32(sections) & 0xFFFFFFFF
        total_size = 16 + len(sections)
        header = struct.pack('<4sBBHLL', IR_MAGIC, IR_MAJOR_VERSION, IR_MINOR_VERSION, 0, total_size, crc)
        return header + sections

def create_solid_basic_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    white = builder.add_paint(Paint.solid(255, 255, 255))
    red = builder.add_paint(Paint.solid(255, 0, 0))
    green = builder.add_paint(Paint.solid(0, 255, 0))
    blue = builder.add_paint(Paint.solid(0, 0, 255))
    yellow = builder.add_paint(Paint.solid(255, 255, 0))
    
    rect1 = builder.add_path(Path().rect(50, 50, 200, 150))
    rect2 = builder.add_path(Path().rect(300, 50, 200, 150))
    rect3 = builder.add_path(Path().rect(550, 50, 200, 150))
    circle1 = builder.add_path(Path().circle(150, 400, 100))
    circle2 = builder.add_path(Path().circle(400, 400, 80))
    circle3 = builder.add_path(Path().circle(650, 400, 60))
    
    builder.clear(255, 255, 255)
    builder.set_fill(red).fill_path(rect1)
    builder.set_fill(green).fill_path(rect2)
    builder.set_fill(blue).fill_path(rect3)
    builder.set_fill(yellow).fill_path(circle1)
    builder.set_fill(red).fill_path(circle2)
    builder.set_fill(blue).fill_path(circle3)
    
    return builder.build(), {
        "scene_id": "fills/solid_basic",
        "description": "Basic solid fill scene",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_nonzero": True}
    }

def create_nested_rects_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    paints = [builder.add_paint(Paint.solid(int(255*(1-i/20)), int(128*(i/20)), int(255*(i/20)), 200)) for i in range(20)]
    paths = []
    cx, cy = 400, 300
    for i in range(20):
        size = 380 - i * 18
        paths.append(builder.add_path(Path().rect(cx-size/2, cy-size/2, size, size)))
    
    builder.clear(32, 32, 32)
    for p, path in zip(paints, paths):
        builder.set_fill(p).fill_path(path)
        
    return builder.build(), {
        "scene_id": "fills/nested_rects",
        "description": "Nested rects performance test",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_nonzero": True}
    }

def create_spiral_circles_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    paints = []
    for i in range(50):
        hue = (i * 7) % 360
        h = hue / 60
        hi = int(h) % 6
        f = h - int(h)
        if hi == 0: r,g,b = 1,f,0
        elif hi == 1: r,g,b = 1-f,1,0
        elif hi == 2: r,g,b = 0,1,f
        elif hi == 3: r,g,b = 0,1-f,1
        elif hi == 4: r,g,b = f,0,1
        else: r,g,b = 1,0,1-f
        paints.append(builder.add_paint(Paint.solid(int(r*255), int(g*255), int(b*255))))
        
    paths = []
    cx, cy = 400, 300
    for i in range(50):
        angle = i * 0.5
        radius = 20 + i * 5
        paths.append(builder.add_path(Path().circle(cx + math.cos(angle)*radius, cy + math.sin(angle)*radius, 15)))
        
    builder.clear(255, 255, 255)
    for p, path in zip(paints, paths):
        builder.set_fill(p).fill_path(path)
        
    return builder.build(), {
        "scene_id": "fills/spiral_circles",
        "description": "Spiral circles test",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_nonzero": True}
    }

def create_gradients_linear_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    
    # Gradient 1: Horizontal Red -> Blue
    g1 = builder.add_paint(Paint.linear(50, 300, 250, 300, [
        GradientStop(0.0, rgba(255, 0, 0)),
        GradientStop(1.0, rgba(0, 0, 255))
    ]))
    
    # Gradient 2: Vertical Green -> Yellow -> Red
    g2 = builder.add_paint(Paint.linear(400, 100, 400, 500, [
        GradientStop(0.0, rgba(0, 255, 0)),
        GradientStop(0.5, rgba(255, 255, 0)),
        GradientStop(1.0, rgba(255, 0, 0))
    ]))
    
    # Gradient 3: Diagonal Rainbow
    g3 = builder.add_paint(Paint.linear(550, 100, 750, 500, [
        GradientStop(0.0, rgba(255, 0, 0)),
        GradientStop(0.2, rgba(255, 255, 0)),
        GradientStop(0.4, rgba(0, 255, 0)),
        GradientStop(0.6, rgba(0, 255, 255)),
        GradientStop(0.8, rgba(0, 0, 255)),
        GradientStop(1.0, rgba(255, 0, 255))
    ]))
    
    rect1 = builder.add_path(Path().rect(50, 100, 200, 400))
    rect2 = builder.add_path(Path().rect(300, 100, 200, 400))
    rect3 = builder.add_path(Path().rect(550, 100, 200, 400))
    
    builder.clear(30, 30, 30)
    builder.set_fill(g1).fill_path(rect1)
    builder.set_fill(g2).fill_path(rect2)
    builder.set_fill(g3).fill_path(rect3)
    
    return builder.build(), {
        "scene_id": "fills/gradients_linear",
        "description": "Linear gradients test",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_nonzero": True}
    }

def create_strokes_curves_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    
    black = builder.add_paint(Paint.solid(0, 0, 0))
    red = builder.add_paint(Paint.solid(255, 0, 0))
    blue = builder.add_paint(Paint.solid(0, 0, 255))
    
    # Path 1: Sine wave
    p1 = Path()
    steps = 100
    w = 700
    h = 100
    for i in range(steps+1):
        x = 50 + (i/steps)*w
        y = 150 + math.sin(i/steps * math.pi * 4) * 50
        if i == 0: p1.move_to(x, y)
        else: p1.line_to(x, y)
    
    # Path 2: Spiral
    p2 = Path().spiral(400, 350, 10, 200, 5)
    
    # Path 3: Shapes with different caps/joins
    rect = builder.add_path(Path().rect(50, 300, 100, 100))
    
    path1_id = builder.add_path(p1)
    path2_id = builder.add_path(p2)
    
    builder.clear(240, 240, 240)
    
    # Stroke Sine Wave
    builder.set_stroke(red, 5.0, StrokeCap.ROUND, StrokeJoin.ROUND)
    builder.stroke_path(path1_id)
    
    # Stroke Spiral
    builder.set_stroke(blue, 2.0, StrokeCap.BUTT, StrokeJoin.MITER)
    builder.stroke_path(path2_id)
    
    # Stroke Rect with thick lines
    builder.set_stroke(black, 15.0, StrokeCap.SQUARE, StrokeJoin.BEVEL)
    builder.stroke_path(rect)
    
    return builder.build(), {
        "scene_id": "strokes/strokes_curves",
        "description": "Stroking curves and shapes",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

def create_noop_scene() -> Tuple[bytes, dict]:
    builder = IrBuilder(800, 600)
    
    # 10,000 pairs of Save/Restore.
    # These are effectively no-ops for rendering but stress the command dispatcher.
    # 2 bytes per op * 2 ops * 10000 = 40KB of commands.
    for _ in range(10000):
        builder.save().restore()
        
    return builder.build(), {
        "scene_id": "validation/noop",
        "description": "10k No-Op Commands for Overhead Measurement",
        "default_width": 800, "default_height": 600,
        "required_features": {}
    }

def main():
    scenes_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    scenes_dir = os.path.join(scenes_dir, 'assets', 'scenes')
    
    scenes = [
        (create_solid_basic_scene, 'fills/solid_basic.irbin'),
        (create_nested_rects_scene, 'fills/nested_rects.irbin'),
        (create_spiral_circles_scene, 'fills/spiral_circles.irbin'),
        (create_gradients_linear_scene, 'fills/gradients_linear.irbin'),
        (create_strokes_curves_scene, 'strokes/strokes_curves.irbin'),
        (create_noop_scene, 'validation/noop.irbin'),
    ]
    
    manifest_entries = []
    
    for generator, rel_path in scenes:
        ir_data, metadata = generator()
        full_path = os.path.join(scenes_dir, rel_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, 'wb') as f: f.write(ir_data)
        
        scene_hash = format(zlib.crc32(ir_data) & 0xFFFFFFFF, '08x')
        manifest_entries.append({
            "scene_id": metadata["scene_id"],
            "ir_path": rel_path,
            "scene_hash": scene_hash,
            "ir_version": f"{IR_MAJOR_VERSION}.{IR_MINOR_VERSION}.0",
            "default_width": metadata["default_width"],
            "default_height": metadata["default_height"],
            "required_features": metadata["required_features"],
            "description": metadata.get("description", "")
        })
        print(f"Generated: {rel_path}")
    
    with open(os.path.join(scenes_dir, 'manifest.json'), 'w') as f:
        json.dump({"version": "1.0.0", "scenes": manifest_entries}, f, indent=2)
    print("Manifest updated.")

if __name__ == '__main__':
    main()
