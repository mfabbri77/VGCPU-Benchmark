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
    """Six uniform shapes on a 3x2 grid: squares (red, green, blue) on the
    top row, circles (cyan, magenta, yellow) on the bottom row."""
    builder = IrBuilder(800, 600)
    red = builder.add_paint(Paint.solid(255, 0, 0))
    green = builder.add_paint(Paint.solid(0, 255, 0))
    blue = builder.add_paint(Paint.solid(0, 0, 255))
    cyan = builder.add_paint(Paint.solid(0, 255, 255))
    magenta = builder.add_paint(Paint.solid(255, 0, 255))
    yellow = builder.add_paint(Paint.solid(255, 255, 0))

    # Pixel-alignment policy (owner, 2026-08-30): these synthetic scenes are
    # NOT antialiasing-quality tests (dedicated AA scenes will come later),
    # so geometry avoids gratuitous fractional coverage. Rect edges sit on
    # integer pixel boundaries; circle centers sit on pixel CENTERS (x.5,
    # y.5) with the radius extended by that half pixel (90 -> 90.5) so the
    # circle is tangent to the four orthogonal pixel boundaries.
    cols = [133, 400, 667]
    side = 180
    radius = 90.5

    squares = [builder.add_path(Path().rect(cx - side / 2, 150 - side / 2, side, side))
               for cx in cols]
    circles = [builder.add_path(Path().circle(cx + 0.5, 450.5, radius)) for cx in cols]

    builder.clear(255, 255, 255)
    builder.set_fill(red).fill_path(squares[0])
    builder.set_fill(green).fill_path(squares[1])
    builder.set_fill(blue).fill_path(squares[2])
    builder.set_fill(cyan).fill_path(circles[0])
    builder.set_fill(magenta).fill_path(circles[1])
    builder.set_fill(yellow).fill_path(circles[2])

    return builder.build(), {
        "scene_id": "fills/solid_basic",
        "description": "Uniform 3x2 grid: RGB squares, CMY circles",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_nonzero": True}
    }

def create_nested_rects_scene() -> Tuple[bytes, dict]:
    """32 nested squares: the outermost is opaque, every following pass
    blends at alpha 0.25 (64/255, the closest 8-bit value). Pixel-aligned
    (even sizes around an integer center) per the alignment policy."""
    builder = IrBuilder(800, 600)
    N = 32
    paints = []
    for i in range(N):
        r = int(255 * (1 - i / N))
        g = int(128 * (i / N))
        b = int(255 * (i / N))
        alpha = 255 if i == 0 else 64
        paints.append(builder.add_paint(Paint.solid(r, g, b, alpha)))

    paths = []
    cx, cy = 400, 300
    for i in range(N):
        size = 384 - i * 12  # 384, 372, ... 12: even -> integer edges
        paths.append(builder.add_path(Path().rect(cx - size / 2, cy - size / 2, size, size)))

    builder.clear(32, 32, 32)
    for p, path in zip(paints, paths):
        builder.set_fill(p).fill_path(path)

    return builder.build(), {
        "scene_id": "fills/nested_rects",
        "description": "32 nested squares, opaque base + 31 passes at alpha 0.25",
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
        # Pixel-alignment policy (see solid_basic): snap each circle center
        # to the nearest pixel CENTER and extend the radius by the half
        # pixel (15 -> 15.5, tangent to the four orthogonal boundaries).
        px = math.floor(cx + math.cos(angle) * radius) + 0.5
        py = math.floor(cy + math.sin(angle) * radius) + 0.5
        paths.append(builder.add_path(Path().circle(px, py, 15.5)))
        
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
    """Owner layout (2026-08-30, rev 2): three full-width red waves
    stacked on three rows (lines / quadratic beziers / cubic beziers),
    uniformly distributed above the squares -- interpenetration with the
    spiral is fine; a LARGE blue spiral centered on the canvas; three
    thick black squares on the bottom row with bevel, miter and round
    joins."""
    builder = IrBuilder(800, 600)

    red = builder.add_paint(Paint.solid(255, 0, 0))
    blue = builder.add_paint(Paint.solid(0, 0, 255))
    black = builder.add_paint(Paint.solid(0, 0, 0))

    # --- Three stacked waves: full width, amplitude 40, six periods.
    # Rows at y = 90 / 210 / 330: uniform vertical spacing, the third
    # row's trough (370) stays clear of the squares (top edge 452).
    X0, X1 = 30, 770
    HALVES = 12  # six periods, ~123px each: same density as before
    AMP = 40

    def wave_lines(yc):
        p = Path()
        n = 4 * HALVES  # four straight segments per half period
        for i in range(n + 1):
            t = i / n
            x = X0 + (X1 - X0) * t
            y = yc - AMP * math.sin(t * math.pi * HALVES)
            (p.move_to if i == 0 else p.line_to)(x, y)
        return p

    def wave_quads(yc):
        # one quadratic per half period; a quad's apex deviation is half
        # the control offset, so control offset 2*AMP -> amplitude AMP
        p = Path().move_to(X0, yc)
        w = (X1 - X0) / HALVES
        for i in range(HALVES):
            sign = -1 if i % 2 == 0 else 1
            p.quad_to(X0 + w * (i + 0.5), yc + sign * 2 * AMP, X0 + w * (i + 1), yc)
        return p

    def wave_cubics(yc):
        # one cubic per half period; both controls offset h deviate 3h/4
        # at t=0.5, so h = AMP*4/3 keeps amplitude AMP
        p = Path().move_to(X0, yc)
        w = (X1 - X0) / HALVES
        h = AMP * 4 / 3
        for i in range(HALVES):
            sign = -1 if i % 2 == 0 else 1
            xa = X0 + w * i
            p.cubic_to(xa + w / 3, yc + sign * h,
                       xa + 2 * w / 3, yc + sign * h,
                       xa + w, yc)
        return p

    p_lines = builder.add_path(wave_lines(90))
    p_quads = builder.add_path(wave_quads(210))
    p_cubics = builder.add_path(wave_cubics(330))

    # --- Large centered spiral: reaches into waves and squares (allowed).
    spiral = builder.add_path(Path().spiral(400, 300, 10, 280, 6))

    # --- Bottom row: three squares, joins bevel / miter / round.
    # Width 16 (even) on integer edges keeps stroke boundaries
    # pixel-aligned per the alignment policy; joins differ only at corners.
    squares = [builder.add_path(Path().rect(cx - 45, 460, 90, 90))
               for cx in (170, 400, 630)]

    builder.clear(240, 240, 240)

    builder.set_stroke(red, 5.0, StrokeCap.ROUND, StrokeJoin.ROUND)
    for pid in (p_lines, p_quads, p_cubics):
        builder.stroke_path(pid)

    builder.set_stroke(blue, 2.0, StrokeCap.BUTT, StrokeJoin.MITER)
    builder.stroke_path(spiral)

    for pid, join in zip(squares, (StrokeJoin.BEVEL, StrokeJoin.MITER, StrokeJoin.ROUND)):
        builder.set_stroke(black, 16.0, StrokeCap.BUTT, join)
        builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": "strokes/strokes_curves",
        "description": "Stacked full-width waves (lines/quads/cubics), large spiral, join squares",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

# ---------------------------------------------------------------------------
# Degenerate stroke scenes (owner request, 2026-08-30). One degeneracy class
# per scene, no class repeated across scenes. Taxonomy follows the stroking
# literature: M. Kilgard, "Polar Stroking: New Theory and Methods for
# Stroking Paths", ACM TOG 39(4), 2020 (cusps, zero-length segments with
# caps, tangent reversals as the canonical hard cases) and D. Nehab,
# "Converting Stroked Primitives to Filled Primitives", SIGGRAPH 2020.
# ---------------------------------------------------------------------------

# Shared stroke-test palette (owner request): green, orange, teal, purple.
DEGEN_PALETTE = ((0, 160, 0), (255, 140, 0), (0, 150, 160), (150, 0, 200))

def create_degen_cusps_scene() -> Tuple[bytes, dict]:
    """Cubic/quadratic cusps: points where the tangent vanishes mid-curve.
    Exact cusp, near-cusp (curvature blowup without singularity), a loop
    (the cusp is the degenerate boundary of the loop family), and a
    quadratic that retraces its own chord (cusp at t=0.5).
    Layout: 2x2 grid, each drawing centered in its 400x300 cell."""
    builder = IrBuilder(800, 600)
    paints = [builder.add_paint(Paint.solid(*rgb)) for rgb in DEGEN_PALETTE]

    # Exact cusp at t=0.5: P3+P2-P1-P0 == 0 makes B'(0.5) vanish.
    # Curve spans x in [cx-150, cx+150], y in [cy-75, cy+75].
    exact = builder.add_path(Path().move_to(50, 75)
                             .cubic_to(350, 275, 50, 275, 350, 75))
    # Near-cusp: same polygon with P2 perturbed 12px -- extreme curvature,
    # no singularity; stresses robustness near the cusp condition.
    near = builder.add_path(Path().move_to(450, 75)
                            .cubic_to(750, 275, 450, 263, 750, 75))
    # Loop: X-crossing control polygon, self-intersecting offset curves.
    # Curve spans y in [cy-75, cy+90], symmetric about cx.
    loop = builder.add_path(Path().move_to(50, 375)
                            .cubic_to(450, 595, -50, 595, 350, 375))
    # Quadratic retrace: control collinear beyond both endpoints; the
    # curve runs out and back over its own chord, cusp at the far end.
    # Visible span x in [cx-50, cx+50].
    retrace = builder.add_path(Path().move_to(550, 450).quad_to(750, 450, 550, 450))

    builder.clear(240, 240, 240)
    for pid, paint in zip((exact, near, loop, retrace), paints):
        builder.set_stroke(paint, 24.0, StrokeCap.ROUND, StrokeJoin.ROUND)
        builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": "strokes/degen_cusps",
        "description": "Cusp/near-cusp/loop cubics and a retraced quadratic on a centered 2x2 grid",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

def create_degen_empty_scene() -> Tuple[bytes, dict]:
    """Zero-length geometry where caps must synthesize ALL the ink.
    Round/square caps on empty segments should produce a dot/square of
    stroke-width size; butt caps should produce nothing (a classic
    cross-engine divergence). Same 2x2 layout and stroke widths as
    degen_short_wide."""
    builder = IrBuilder(800, 600)
    paints = [builder.add_paint(Paint.solid(*rgb)) for rgb in DEGEN_PALETTE]

    cases = [
        # zero-length line, width 120, round: a 120px dot
        (Path().move_to(200, 150).line_to(200, 150), 120.0, StrokeCap.ROUND),
        # zero-length line, width 120, square: a 120px square dot
        (Path().move_to(600, 150).line_to(600, 150), 120.0, StrokeCap.SQUARE),
        # zero-length line, width 100, butt: nothing (spec-wise)
        (Path().move_to(200, 430).line_to(200, 430), 100.0, StrokeCap.BUTT),
        # all-coincident cubic, width 80, round: dot iff caps synthesized
        (Path().move_to(600, 430).cubic_to(600, 430, 600, 430, 600, 430), 80.0, StrokeCap.ROUND),
    ]
    builder.clear(240, 240, 240)
    for paint, (path, width, cap) in zip(paints, cases):
        pid = builder.add_path(path)
        builder.set_stroke(paint, width, cap, StrokeJoin.ROUND)
        builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": "strokes/degen_empty",
        "description": "Zero-length segments: caps must synthesize the geometry",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

def create_degen_short_wide_scene() -> Tuple[bytes, dict]:
    """Finite but tiny curves stroked with width far exceeding their arc
    length (ratios 13:1 up to 40:1). The offset construction degenerates;
    engines differ in wafer orientation and cap dominance."""
    builder = IrBuilder(800, 600)
    paints = [builder.add_paint(Paint.solid(*rgb)) for rgb in DEGEN_PALETTE]

    cases = [
        # 3px line, width 120, butt: a 3x120 wafer perpendicular to the line
        (Path().move_to(200, 150).line_to(203, 150), 120.0, StrokeCap.BUTT),
        # 3px line, width 120, round: cap-dominated lozenge ~123px
        (Path().move_to(600, 150).line_to(603, 150), 120.0, StrokeCap.ROUND),
        # ~6px cubic wiggle, width 100, round
        (Path().move_to(200, 430).cubic_to(202, 427, 204, 433, 206, 430), 100.0, StrokeCap.ROUND),
        # ~4px quadratic, width 80, square
        (Path().move_to(600, 430).quad_to(602, 426, 604, 430), 80.0, StrokeCap.SQUARE),
    ]
    builder.clear(240, 240, 240)
    for paint, (path, width, cap) in zip(paints, cases):
        pid = builder.add_path(path)
        builder.set_stroke(paint, width, cap, StrokeJoin.ROUND)
        builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": "strokes/degen_short_wide",
        "description": "Tiny curves with stroke width >> arc length (up to 40:1)",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

def create_degen_reversal_scene() -> Tuple[bytes, dict]:
    """Tangent reversals at joins: exact 180-degree retrace and near-180
    turns where the miter length ~ 1/sin(theta/2) explodes and must be
    clamped by the miter limit. Round join as the stable control."""
    builder = IrBuilder(800, 600)
    paints = [builder.add_paint(Paint.solid(*rgb)) for rgb in DEGEN_PALETTE[:3]]

    cases = [
        # exact 180: out 400px and straight back, miter join
        (Path().move_to(150, 120).line_to(550, 120).line_to(150, 120), StrokeJoin.MITER),
        # near-180 (~2 deg): miter length ~57x half-width -> limit clamp
        (Path().move_to(150, 300).line_to(550, 300).line_to(154, 286), StrokeJoin.MITER),
        # same near-180 with round join: the stable reference behavior
        (Path().move_to(150, 480).line_to(550, 480).line_to(154, 466), StrokeJoin.ROUND),
    ]
    builder.clear(240, 240, 240)
    for paint, (path, join) in zip(paints, cases):
        pid = builder.add_path(path)
        builder.set_stroke(paint, 30.0, StrokeCap.BUTT, join)
        builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": "strokes/degen_reversal",
        "description": "180-degree retrace and near-180 miter-limit explosions",
        "default_width": 800, "default_height": 600,
        "required_features": {"needs_stroke": True}
    }

# ---------------------------------------------------------------------------
# Real-world scenes converted from vendored SVGs (owner requests, 2026-08-30).
# The minimal SVG subset needed by those files is parsed here: groups with
# translate/scale/matrix transforms (baked into coordinates), style or
# attribute fill/stroke/stroke-width/stroke-linecap/fill-rule, elements
# path (m/l/h/v/c/s/z, abs + rel), polygon and rect. Stroke widths scale
# by the uniform matrix factor sqrt(|det|).
# ---------------------------------------------------------------------------

def convert_svg_scene(svg_name, scene_id, description, features) -> Tuple[bytes, dict]:
    import re
    import xml.etree.ElementTree as ET

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    tree = ET.parse(os.path.join(repo, 'assets', 'svg', svg_name))
    root = tree.getroot()

    NUM = re.compile(r'[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?')

    def mat_mul(a, b):  # 2x3 affine (a c e / b d f), column-vector convention
        return (a[0]*b[0] + a[2]*b[1],        a[1]*b[0] + a[3]*b[1],
                a[0]*b[2] + a[2]*b[3],        a[1]*b[2] + a[3]*b[3],
                a[0]*b[4] + a[2]*b[5] + a[4], a[1]*b[4] + a[3]*b[5] + a[5])

    def parse_transform(s):
        m = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
        for name, args in re.findall(r'(\w+)\s*\(([^)]*)\)', s or ''):
            v = [float(x) for x in NUM.findall(args)]
            if name == 'translate':
                t = (1.0, 0.0, 0.0, 1.0, v[0], v[1] if len(v) > 1 else 0.0)
            elif name == 'scale':
                t = (v[0], 0.0, 0.0, v[1] if len(v) > 1 else v[0], 0.0, 0.0)
            elif name == 'matrix':
                t = tuple(v)
            else:
                raise ValueError(f'unsupported transform: {name}')
            m = mat_mul(m, t)
        return m

    def hex_color(s):
        h = s.strip().lstrip('#')
        if len(h) in (3, 4):
            h = ''.join(ch * 2 for ch in h)
        if len(h) == 6:
            h += 'ff'
        return tuple(int(h[k:k + 2], 16) for k in (0, 2, 4, 6))

    def parse_style(el):
        style = {}
        for part in (el.get('style') or '').split(';'):
            if ':' in part:
                k, v = part.split(':', 1)
                style[k.strip()] = v.strip()
        for k in ('fill', 'stroke', 'stroke-width', 'stroke-linecap', 'fill-rule'):
            if el.get(k) is not None:
                style[k] = el.get(k)
        return style

    # Fit: viewBox -> 800x600 canvas, uniform scale, centered.
    vb = [float(x) for x in NUM.findall(root.get('viewBox'))]
    W, H = 800, 600
    s = min(W / vb[2], H / vb[3])
    fit = (s, 0.0, 0.0, s, (W - vb[2] * s) / 2 - vb[0] * s, (H - vb[3] * s) / 2 - vb[1] * s)

    def parse_path_data(d, m):
        p = Path()
        toks = re.findall(r'[MmLlHhVvCcSsQqTtAaZz]|' + NUM.pattern, d)
        i = 0
        cmd = None
        cx = cy = sx = sy = 0.0  # current point / subpath start (user units)
        pcx = pcy = None         # last cubic control (for s/S reflection)

        def nums(n):
            nonlocal i
            v = [float(t) for t in toks[i:i + n]]
            i += n
            return v

        def emit(fn, *pts):
            fn(*[c for x, y in pts for c in apply_m(x, y)])

        def apply_m(x, y):
            return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])

        while i < len(toks):
            if toks[i].isalpha():
                cmd = toks[i]
                i += 1
            rel = cmd.islower()
            c = cmd.lower()
            if c == 'z':
                p.close()
                cx, cy = sx, sy
                pcx = pcy = None
                continue
            if c == 'm':
                x, y = nums(2)
                if rel: x += cx; y += cy
                cx, cy = sx, sy = x, y
                emit(p.move_to, (x, y))
                cmd = 'l' if rel else 'L'  # extra pairs are implicit linetos
                pcx = pcy = None
            elif c == 'l':
                x, y = nums(2)
                if rel: x += cx; y += cy
                cx, cy = x, y
                emit(p.line_to, (x, y))
                pcx = pcy = None
            elif c == 'h':
                (x,) = nums(1)
                if rel: x += cx
                cx = x
                emit(p.line_to, (x, cy))
                pcx = pcy = None
            elif c == 'v':
                (y,) = nums(1)
                if rel: y += cy
                cy = y
                emit(p.line_to, (cx, y))
                pcx = pcy = None
            elif c == 'c':
                x1, y1, x2, y2, x, y = nums(6)
                if rel:
                    x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy
                emit(p.cubic_to, (x1, y1), (x2, y2), (x, y))
                pcx, pcy = x2, y2
                cx, cy = x, y
            elif c == 's':
                x2, y2, x, y = nums(4)
                if rel:
                    x2 += cx; y2 += cy; x += cx; y += cy
                x1 = 2 * cx - pcx if pcx is not None else cx
                y1 = 2 * cy - pcy if pcy is not None else cy
                emit(p.cubic_to, (x1, y1), (x2, y2), (x, y))
                pcx, pcy = x2, y2
                cx, cy = x, y
            else:
                raise ValueError(f'unsupported path command: {cmd}')
        return p

    builder = IrBuilder(W, H)
    paint_cache = {}
    path_cache = {}

    def paint_id(rgba):
        if rgba not in paint_cache:
            paint_cache[rgba] = builder.add_paint(Paint.solid(*rgba))
        return paint_cache[rgba]

    draws = []  # (kind, paint_rgba, path_id, width_or_rule, cap) in document order

    def add_draws(el, m, pid):
        style = parse_style(el)
        fill = style.get('fill', '#000000')
        if fill != 'none':
            rule = FillRule.EVEN_ODD if style.get('fill-rule') == 'evenodd' else FillRule.NON_ZERO
            draws.append(('fill', hex_color(fill), pid, rule, None))
        stroke = style.get('stroke', 'none')
        if stroke != 'none':
            wscale = math.sqrt(abs(m[0] * m[3] - m[1] * m[2]))
            width = float(style.get('stroke-width', '1')) * wscale
            cap = StrokeCap.ROUND if style.get('stroke-linecap') == 'round' else StrokeCap.BUTT
            draws.append(('stroke', hex_color(stroke), pid, width, cap))

    def apply_pt(m, x, y):
        return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])

    def walk(el, m):
        tag = el.tag.split('}')[-1]
        m = mat_mul(m, parse_transform(el.get('transform')))
        if tag == 'path':
            d = el.get('d')
            key = (d, m)
            if key not in path_cache:
                path_cache[key] = builder.add_path(parse_path_data(d, m))
            add_draws(el, m, path_cache[key])
        elif tag == 'polygon':
            pts = [float(v) for v in NUM.findall(el.get('points'))]
            p = Path()
            for k in range(0, len(pts), 2):
                (p.move_to if k == 0 else p.line_to)(*apply_pt(m, pts[k], pts[k + 1]))
            p.close()
            add_draws(el, m, builder.add_path(p))
        elif tag == 'rect':
            x = float(el.get('x', '0')); y = float(el.get('y', '0'))
            w = float(el.get('width')); h = float(el.get('height'))
            p = Path()
            corners = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
            for k, (px, py) in enumerate(corners):
                (p.move_to if k == 0 else p.line_to)(*apply_pt(m, px, py))
            p.close()
            add_draws(el, m, builder.add_path(p))
        for child in el:
            walk(child, m)

    walk(root, fit)

    builder.clear(255, 255, 255)
    for kind, rgba, pid, arg, cap in draws:
        if kind == 'fill':
            builder.set_fill(paint_id(rgba), arg).fill_path(pid)
        else:
            builder.set_stroke(paint_id(rgba), arg, cap, StrokeJoin.MITER)
            builder.stroke_path(pid)

    return builder.build(), {
        "scene_id": scene_id,
        "description": description,
        "default_width": W, "default_height": H,
        "required_features": features
    }

def create_tiger_scene() -> Tuple[bytes, dict]:
    return convert_svg_scene(
        'tiger.svg', 'complex/tiger',
        "Ghostscript tiger (305 SVG paths, solid fills/strokes, cubic-heavy)",
        {"needs_nonzero": True, "needs_stroke": True})

def create_paris_scene() -> Tuple[bytes, dict]:
    return convert_svg_scene(
        'paris.svg', 'complex/paris',
        "Paris map (3056 polygons, ~41k vertices, evenodd building paths)",
        {"needs_nonzero": True})

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
        (create_degen_cusps_scene, 'strokes/degen_cusps.irbin'),
        (create_degen_empty_scene, 'strokes/degen_empty.irbin'),
        (create_degen_short_wide_scene, 'strokes/degen_short_wide.irbin'),
        (create_degen_reversal_scene, 'strokes/degen_reversal.irbin'),
        (create_tiger_scene, 'complex/tiger.irbin'),
        (create_paris_scene, 'complex/paris.irbin'),
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
