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

@dataclass
class Paint:
    """Paint definition (solid color or gradient)."""
    paint_type: PaintType = PaintType.SOLID
    color: int = 0xFF000000  # RGBA8 (A is high byte)
    
    @staticmethod
    def solid(r: int, g: int, b: int, a: int = 255) -> 'Paint':
        """Create a solid color paint from RGBA components (0-255)."""
        color = r | (g << 8) | (b << 16) | (a << 24)
        return Paint(paint_type=PaintType.SOLID, color=color)

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
        # Bezier circle approximation constant
        k = 0.5522847498
        
        self.move_to(cx + r, cy)
        self.cubic_to(cx + r, cy + r*k, cx + r*k, cy + r, cx, cy + r)
        self.cubic_to(cx - r*k, cy + r, cx - r, cy + r*k, cx - r, cy)
        self.cubic_to(cx - r, cy - r*k, cx - r*k, cy - r, cx, cy - r)
        self.cubic_to(cx + r*k, cy - r, cx + r, cy - r*k, cx + r, cy)
        return self.close()

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
        """Add a paint and return its index."""
        idx = len(self.paints)
        self.paints.append(paint)
        return idx
    
    def add_path(self, path: Path) -> int:
        """Add a path and return its index."""
        idx = len(self.paths)
        self.paths.append(path)
        return idx
    
    def clear(self, r: int, g: int, b: int, a: int = 255):
        """Add a clear command."""
        rgba = r | (g << 8) | (b << 16) | (a << 24)
        self.commands.append(Command(Opcode.CLEAR, struct.pack('<I', rgba)))
        return self
    
    def set_fill(self, paint_id: int, fill_rule: FillRule = FillRule.NON_ZERO):
        """Set the current fill paint and rule."""
        args = struct.pack('<HB', paint_id, fill_rule)
        self.commands.append(Command(Opcode.SET_FILL, args))
        return self
    
    def fill_path(self, path_id: int):
        """Fill a path."""
        args = struct.pack('<H', path_id)
        self.commands.append(Command(Opcode.FILL_PATH, args))
        return self
    
    def set_stroke(self, paint_id: int, width: float, cap: int = 0, join: int = 0):
        """Set the current stroke paint and parameters."""
        opts = cap | (join << 2)
        args = struct.pack('<HfB', paint_id, width, opts)
        self.commands.append(Command(Opcode.SET_STROKE, args))
        return self
    
    def stroke_path(self, path_id: int):
        """Stroke a path."""
        args = struct.pack('<H', path_id)
        self.commands.append(Command(Opcode.STROKE_PATH, args))
        return self
    
    def save(self):
        """Push graphics state."""
        self.commands.append(Command(Opcode.SAVE))
        return self
    
    def restore(self):
        """Pop graphics state."""
        self.commands.append(Command(Opcode.RESTORE))
        return self
    
    def _build_paint_section(self) -> bytes:
        """Build the paint section payload."""
        data = struct.pack('<H', len(self.paints))  # paint count
        for paint in self.paints:
            # Paint entry: type(u8), color(u32)
            data += struct.pack('<BI', paint.paint_type, paint.color)
        return data
    
    def _build_path_section(self) -> bytes:
        """Build the path section payload."""
        data = struct.pack('<H', len(self.paths))  # path count
        for path in self.paths:
            # Path header: verb_count(u16), point_count(u16)
            data += struct.pack('<HH', len(path.verbs), len(path.points))
            # Verbs
            for verb in path.verbs:
                data += struct.pack('<B', verb)
            # Points (f32 pairs)
            for pt in path.points:
                data += struct.pack('<f', pt)
        return data
    
    def _build_command_section(self) -> bytes:
        """Build the command section payload."""
        data = b''
        for cmd in self.commands:
            data += struct.pack('<B', cmd.opcode) + cmd.args
        # End marker
        data += struct.pack('<B', Opcode.END)
        return data
    
    def _build_section(self, section_type: SectionType, payload: bytes) -> bytes:
        """Build a complete section with header."""
        # Section header: type(u8), reserved(u8), length(u32)
        header_size = 6
        length = header_size + len(payload)
        header = struct.pack('<BBL', section_type, 0, length)
        return header + payload
    
    def build(self) -> bytes:
        """Build the complete IR binary."""
        # Build sections
        sections = b''
        
        if self.paints:
            sections += self._build_section(SectionType.PAINT, self._build_paint_section())
        
        if self.paths:
            sections += self._build_section(SectionType.PATH, self._build_path_section())
        
        if self.commands:
            sections += self._build_section(SectionType.COMMAND, self._build_command_section())
        
        # Calculate CRC of sections
        crc = zlib.crc32(sections) & 0xFFFFFFFF
        
        # Build header
        total_size = 16 + len(sections)
        header = struct.pack('<4sBBHLL',
            IR_MAGIC,
            IR_MAJOR_VERSION,
            IR_MINOR_VERSION,
            0,  # reserved
            total_size,
            crc
        )
        
        return header + sections

def create_solid_basic_scene() -> Tuple[bytes, dict]:
    """Create a basic solid fill scene with rectangles and circles."""
    builder = IrBuilder(800, 600)
    
    # Add paints
    white = builder.add_paint(Paint.solid(255, 255, 255, 255))
    red = builder.add_paint(Paint.solid(255, 0, 0, 255))
    green = builder.add_paint(Paint.solid(0, 255, 0, 255))
    blue = builder.add_paint(Paint.solid(0, 0, 255, 255))
    yellow = builder.add_paint(Paint.solid(255, 255, 0, 255))
    
    # Add paths
    rect1 = builder.add_path(Path().rect(50, 50, 200, 150))
    rect2 = builder.add_path(Path().rect(300, 50, 200, 150))
    rect3 = builder.add_path(Path().rect(550, 50, 200, 150))
    circle1 = builder.add_path(Path().circle(150, 400, 100))
    circle2 = builder.add_path(Path().circle(400, 400, 80))
    circle3 = builder.add_path(Path().circle(650, 400, 60))
    
    # Build command stream
    builder.clear(255, 255, 255, 255)  # White background
    
    builder.set_fill(red).fill_path(rect1)
    builder.set_fill(green).fill_path(rect2)
    builder.set_fill(blue).fill_path(rect3)
    
    builder.set_fill(yellow).fill_path(circle1)
    builder.set_fill(red).fill_path(circle2)
    builder.set_fill(blue).fill_path(circle3)
    
    metadata = {
        "scene_id": "fills/solid_basic",
        "description": "Basic solid fill scene with rectangles and circles",
        "default_width": 800,
        "default_height": 600,
        "required_features": {
            "needs_nonzero": True
        }
    }
    
    return builder.build(), metadata

def create_nested_rects_scene() -> Tuple[bytes, dict]:
    """Create a scene with many nested rectangles for performance testing."""
    builder = IrBuilder(800, 600)
    
    # Add paints (gradient of colors)
    paints = []
    for i in range(20):
        r = int(255 * (1 - i/20))
        g = int(128 * (i/20))
        b = int(255 * (i/20))
        paints.append(builder.add_paint(Paint.solid(r, g, b, 200)))
    
    # Add nested rectangle paths
    paths = []
    cx, cy = 400, 300
    for i in range(20):
        size = 380 - i * 18
        x = cx - size/2
        y = cy - size/2
        paths.append(builder.add_path(Path().rect(x, y, size, size)))
    
    # Build command stream
    builder.clear(32, 32, 32, 255)  # Dark background
    
    for i, (paint_id, path_id) in enumerate(zip(paints, paths)):
        builder.set_fill(paint_id).fill_path(path_id)
    
    metadata = {
        "scene_id": "fills/nested_rects",
        "description": "Performance test with 20 nested rectangles",
        "default_width": 800,
        "default_height": 600,
        "required_features": {
            "needs_nonzero": True
        }
    }
    
    return builder.build(), metadata

def create_spiral_circles_scene() -> Tuple[bytes, dict]:
    """Create a scene with circles arranged in a spiral pattern."""
    import math
    
    builder = IrBuilder(800, 600)
    
    # Add paints
    paints = []
    for i in range(50):
        hue = (i * 7) % 360
        # Simple HSV to RGB (hue only, full saturation/value)
        h = hue / 60
        hi = int(h) % 6
        f = h - int(h)
        if hi == 0: r, g, b = 1, f, 0
        elif hi == 1: r, g, b = 1-f, 1, 0
        elif hi == 2: r, g, b = 0, 1, f
        elif hi == 3: r, g, b = 0, 1-f, 1
        elif hi == 4: r, g, b = f, 0, 1
        else: r, g, b = 1, 0, 1-f
        paints.append(builder.add_paint(Paint.solid(int(r*255), int(g*255), int(b*255), 255)))
    
    # Add spiral circles
    paths = []
    cx, cy = 400, 300
    for i in range(50):
        angle = i * 0.5
        radius = 20 + i * 5
        x = cx + math.cos(angle) * radius
        y = cy + math.sin(angle) * radius
        paths.append(builder.add_path(Path().circle(x, y, 15)))
    
    # Build command stream
    builder.clear(255, 255, 255, 255)  # White background
    
    for paint_id, path_id in zip(paints, paths):
        builder.set_fill(paint_id).fill_path(path_id)
    
    metadata = {
        "scene_id": "fills/spiral_circles",
        "description": "50 circles in a spiral pattern with rainbow colors",
        "default_width": 800,
        "default_height": 600,
        "required_features": {
            "needs_nonzero": True
        }
    }
    
    return builder.build(), metadata

def main():
    """Generate all IR scene files."""
    scenes_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    scenes_dir = os.path.join(scenes_dir, 'assets', 'scenes')
    
    scenes = [
        (create_solid_basic_scene, 'fills/solid_basic.irbin'),
        (create_nested_rects_scene, 'fills/nested_rects.irbin'),
        (create_spiral_circles_scene, 'fills/spiral_circles.irbin'),
    ]
    
    manifest_entries = []
    
    for generator, rel_path in scenes:
        ir_data, metadata = generator()
        
        # Write IR file
        full_path = os.path.join(scenes_dir, rel_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, 'wb') as f:
            f.write(ir_data)
        
        # Calculate hash (using CRC32 for simplicity, should be SHA-256 in production)
        scene_hash = format(zlib.crc32(ir_data) & 0xFFFFFFFF, '08x')
        
        # Add to manifest
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
        
        print(f"Generated: {rel_path} ({len(ir_data)} bytes, hash: {scene_hash})")
    
    # Write manifest
    manifest_path = os.path.join(scenes_dir, 'manifest.json')
    with open(manifest_path, 'w') as f:
        json.dump({
            "version": "1.0.0",
            "scenes": manifest_entries
        }, f, indent=2)
    
    print(f"\nManifest written to: {manifest_path}")
    print(f"Total scenes: {len(manifest_entries)}")

if __name__ == '__main__':
    main()
