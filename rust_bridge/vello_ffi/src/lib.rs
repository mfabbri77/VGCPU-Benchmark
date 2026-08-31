// Vello CPU FFI Bridge (vello_cpu 0.0.4)
// Blueprint Reference: backends/vello.md

use vello_cpu::{RenderContext, Pixmap, PaintType};
use vello_cpu::kurbo::{BezPath, Cap, Join, Rect, Stroke};
use vello_cpu::peniko::{Color, ColorStop, Gradient, Fill};
use vello_cpu::peniko::color::DynamicColor;

/// Opaque handle to Vello RenderContext
pub struct VloSurface {
    ctx: RenderContext,
    width: u16,
    height: u16,
}

/// Helper for path construction
pub struct VloPath {
    path: BezPath,
}

pub struct VloPaintBuf {
    pub paint: PaintType,
}

#[no_mangle]
pub extern "C" fn vlo_paint_create_solid(r: u8, g: u8, b: u8, a: u8) -> *mut VloPaintBuf {
    let paint = PaintType::Solid(Color::from_rgba8(r, g, b, a));
    Box::into_raw(Box::new(VloPaintBuf { paint }))
}

#[no_mangle]
pub extern "C" fn vlo_paint_create_gradient(
    kind: i32,
    x0: f32, y0: f32, x1: f32, y1: f32,
    offsets: *const f32,
    colors: *const u32,
    nstops: i32,
) -> *mut VloPaintBuf {
    if offsets.is_null() || colors.is_null() || nstops <= 0 { return std::ptr::null_mut(); }
    let offsets_slice = unsafe { std::slice::from_raw_parts(offsets, nstops as usize) };
    let colors_slice = unsafe { std::slice::from_raw_parts(colors, nstops as usize) };

    let mut stops = Vec::with_capacity(nstops as usize);
    for i in 0..nstops as usize {
        let c = colors_slice[i];
        let r_val = (c & 0xFF) as u8;
        let g_val = ((c >> 8) & 0xFF) as u8;
        let b_val = ((c >> 16) & 0xFF) as u8;
        let a_val = ((c >> 24) & 0xFF) as u8;
        let color = Color::from_rgba8(r_val, g_val, b_val, a_val);
        stops.push(ColorStop {
            offset: offsets_slice[i],
            color: DynamicColor::from_alpha_color(color),
        });
    }

    let gradient = if kind == 0 {
        Gradient::new_linear((x0 as f64, y0 as f64), (x1 as f64, y1 as f64))
    } else {
        Gradient::new_radial((x0 as f64, y0 as f64), x1)
    };
    let gradient = gradient.with_stops(&stops[..]);
    Box::into_raw(Box::new(VloPaintBuf { paint: PaintType::Gradient(gradient) }))
}

#[no_mangle]
pub extern "C" fn vlo_paint_destroy(ptr: *mut VloPaintBuf) {
    if ptr.is_null() { return; }
    unsafe { drop(Box::from_raw(ptr)); }
}

#[no_mangle]
pub extern "C" fn vlo_draw_fill_with_paint(
    surf: *mut VloSurface,
    path_ptr: *const VloPath,
    paint_ptr: *const VloPaintBuf,
    even_odd: bool
) {
    if surf.is_null() || path_ptr.is_null() || paint_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path = unsafe { &*path_ptr };
    let paint_buf = unsafe { &*paint_ptr };
    surface.ctx.set_fill_rule(if even_odd { Fill::EvenOdd } else { Fill::NonZero });
    surface.ctx.set_paint(paint_buf.paint.clone());
    surface.ctx.fill_path(&path.path);
}
#[no_mangle]
pub extern "C" fn vlo_draw_stroke_with_paint(
    surf: *mut VloSurface,
    path_ptr: *const VloPath,
    paint_ptr: *const VloPaintBuf,
    width: f32,
    cap: i32,
    join: i32,
    dashes: *const f32,
    ndash: i32,
    dash_phase: f32
) {
    if surf.is_null() || path_ptr.is_null() || paint_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path = unsafe { &*path_ptr };
    let paint_buf = unsafe { &*paint_ptr };

    let stroke = Stroke::new(width as f64)
        .with_caps(match cap {
            1 => Cap::Round,
            2 => Cap::Square,
            _ => Cap::Butt,
        })
        .with_join(match join {
            1 => Join::Round,
            2 => Join::Bevel,
            _ => Join::Miter,
        });

    surface.ctx.set_stroke(stroke);
    surface.ctx.set_paint(paint_buf.paint.clone());

    if !dashes.is_null() && ndash > 0 {
        let dashes_slice = unsafe { std::slice::from_raw_parts(dashes, ndash as usize) };
        let dashes_f64: Vec<f64> = dashes_slice.iter().map(|&x| x as f64).collect();
        let dashed_path: BezPath = vello_cpu::kurbo::dash(path.path.elements().iter().copied(), dash_phase as f64, &dashes_f64).collect();
        surface.ctx.stroke_path(&dashed_path);
    } else {
        surface.ctx.stroke_path(&path.path);
    }
}

// ============================================================================
// Surface Management
// ============================================================================

#[no_mangle]
pub extern "C" fn vlo_create(width: i32, height: i32) -> *mut VloSurface {
    let w = width as u16;
    let h = height as u16;
    let ctx = RenderContext::new(w, h);
    Box::into_raw(Box::new(VloSurface { ctx, width: w, height: h }))
}

#[no_mangle]
pub extern "C" fn vlo_destroy(ptr: *mut VloSurface) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn vlo_clear(ptr: *mut VloSurface, r: u8, g: u8, b: u8, a: u8) {
    if ptr.is_null() { return; }
    let surf = unsafe { &mut *ptr };
    let color = Color::from_rgba8(r, g, b, a);
    
    // vello_cpu 0.0.4 doesn't have clear(), but we can fill a rect covering the entire surface
    surf.ctx.set_paint(color);
    surf.ctx.fill_rect(&Rect::new(0.0, 0.0, surf.width as f64, surf.height as f64));
}

#[no_mangle]
pub extern "C" fn vlo_get_pixels(ptr: *mut VloSurface, out_buf: *mut u32) {
    if ptr.is_null() || out_buf.is_null() { return; }
    let surf = unsafe { &mut *ptr };
    
    let mut pixmap = Pixmap::new(surf.width, surf.height);
    surf.ctx.render_to_pixmap(&mut pixmap);
    
    let data = pixmap.data();
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr() as *const u32, out_buf, surf.width as usize * surf.height as usize);
    }
}

// ============================================================================
// Path Construction
// ============================================================================

#[no_mangle]
pub extern "C" fn vlo_path_create() -> *mut VloPath {
    Box::into_raw(Box::new(VloPath { path: BezPath::new() }))
}

#[no_mangle]
pub extern "C" fn vlo_path_destroy(ptr: *mut VloPath) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn vlo_path_move_to(ptr: *mut VloPath, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let p = unsafe { &mut *ptr };
    p.path.move_to((x as f64, y as f64));
}

#[no_mangle]
pub extern "C" fn vlo_path_line_to(ptr: *mut VloPath, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let p = unsafe { &mut *ptr };
    p.path.line_to((x as f64, y as f64));
}

#[no_mangle]
pub extern "C" fn vlo_path_quad_to(ptr: *mut VloPath, cx: f32, cy: f32, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let p = unsafe { &mut *ptr };
    p.path.quad_to((cx as f64, cy as f64), (x as f64, y as f64));
}

#[no_mangle]
pub extern "C" fn vlo_path_cubic_to(ptr: *mut VloPath, c1x: f32, c1y: f32, c2x: f32, c2y: f32, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let p = unsafe { &mut *ptr };
    p.path.curve_to((c1x as f64, c1y as f64), (c2x as f64, c2y as f64), (x as f64, y as f64));
}

#[no_mangle]
pub extern "C" fn vlo_path_close(ptr: *mut VloPath) {
    if ptr.is_null() { return; }
    let p = unsafe { &mut *ptr };
    p.path.close_path();
}

// ============================================================================
// Drawing Operations
// ============================================================================

#[no_mangle]
pub extern "C" fn vlo_fill_path(
    surf: *mut VloSurface,
    path_ptr: *mut VloPath,
    r: u8, g: u8, b: u8, a: u8,
    _even_odd: bool
) {
    if surf.is_null() || path_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let p = unsafe { &*path_ptr };
    
    surface.ctx.set_paint(Color::from_rgba8(r, g, b, a));
    surface.ctx.fill_path(&p.path);
}


/// Fill a path with a gradient.
/// kind: 0 = linear (x0,y0 -> x1,y1), 1 = radial (center x0,y0, radius x1).
/// colors: packed r | g<<8 | b<<16 | a<<24 per stop (true RGBA -- vello's
/// pixmap is already contract byte order, no channel swap needed).
#[no_mangle]
pub extern "C" fn vlo_fill_path_gradient(
    surf: *mut VloSurface,
    path_ptr: *mut VloPath,
    kind: i32,
    x0: f32, y0: f32, x1: f32, y1: f32,
    offsets: *const f32,
    colors: *const u32,
    nstops: i32,
    _even_odd: bool,
) {
    if surf.is_null() || path_ptr.is_null() || offsets.is_null() || colors.is_null() {
        return;
    }
    if nstops <= 0 {
        return;
    }
    let surface = unsafe { &mut *surf };
    let p = unsafe { &*path_ptr };

    let n = nstops as usize;
    let offs = unsafe { std::slice::from_raw_parts(offsets, n) };
    let cols = unsafe { std::slice::from_raw_parts(colors, n) };
    let stops: Vec<ColorStop> = (0..n)
        .map(|i| {
            let c = cols[i];
            let color = Color::from_rgba8(
                (c & 0xFF) as u8,
                ((c >> 8) & 0xFF) as u8,
                ((c >> 16) & 0xFF) as u8,
                ((c >> 24) & 0xFF) as u8,
            );
            ColorStop { offset: offs[i], color: DynamicColor::from_alpha_color(color) }
        })
        .collect();

    let gradient = if kind == 0 {
        Gradient::new_linear((x0 as f64, y0 as f64), (x1 as f64, y1 as f64))
    } else {
        Gradient::new_radial((x0 as f64, y0 as f64), x1)
    };
    let gradient = gradient.with_stops(&stops[..]);

    surface.ctx.set_paint(PaintType::Gradient(gradient));
    surface.ctx.fill_path(&p.path);
}
#[no_mangle]
pub extern "C" fn vlo_stroke_path(
    surf: *mut VloSurface,
    path_ptr: *mut VloPath,
    r: u8, g: u8, b: u8, a: u8,
    width: f32,
    cap: i32,
    join: i32,
    dashes: *const f32,
    ndash: i32,
    dash_phase: f32
) {
    if surf.is_null() || path_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let p = unsafe { &*path_ptr };

    let stroke = Stroke::new(width as f64)
        .with_caps(match cap {
            1 => Cap::Round,
            2 => Cap::Square,
            _ => Cap::Butt,
        })
        .with_join(match join {
            1 => Join::Round,
            2 => Join::Bevel,
            _ => Join::Miter,
        });

    if !dashes.is_null() && ndash > 0 {
        let dashes_slice = unsafe { std::slice::from_raw_parts(dashes, ndash as usize) };
        let dashes_f64: Vec<f64> = dashes_slice.iter().map(|&x| x as f64).collect();
        let dashed_path: BezPath = vello_cpu::kurbo::dash(p.path.elements().iter().copied(), dash_phase as f64, &dashes_f64).collect();
        surface.ctx.set_stroke(stroke);
        surface.ctx.set_paint(Color::from_rgba8(r, g, b, a));
        surface.ctx.stroke_path(&dashed_path);
    } else {
        surface.ctx.set_stroke(stroke);
        surface.ctx.set_paint(Color::from_rgba8(r, g, b, a));
        surface.ctx.stroke_path(&p.path);
    }
}

#[no_mangle]
pub extern "C" fn vlo_clip_push(surf: *mut VloSurface, path_ptr: *mut VloPath) {
    if surf.is_null() || path_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let p = unsafe { &*path_ptr };
    surface.ctx.push_layer(Some(&p.path), None, None, None);
}

#[no_mangle]
pub extern "C" fn vlo_clip_pop(surf: *mut VloSurface) {
    if surf.is_null() { return; }
    let surface = unsafe { &mut *surf };
    surface.ctx.pop_layer();
}

#[no_mangle]
pub extern "C" fn vlo_fill_rect(
    surf: *mut VloSurface,
    x: f32, y: f32, w: f32, h: f32,
    r: u8, g: u8, b: u8, a: u8
) {
    if surf.is_null() { return; }
    let surface = unsafe { &mut *surf };
    surface.ctx.set_paint(Color::from_rgba8(r, g, b, a));
    surface.ctx.fill_rect(&Rect::new(x as f64, y as f64, (x + w) as f64, (y + h) as f64));
}
