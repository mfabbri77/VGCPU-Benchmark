// Vello CPU FFI Bridge (vello_cpu 0.0.4)
// Blueprint Reference: backends/vello.md

use vello_cpu::{RenderContext, Pixmap};
use vello_cpu::kurbo::{BezPath, Rect};
use vello_cpu::peniko::Color;

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

#[no_mangle]
pub extern "C" fn vlo_stroke_path(
    surf: *mut VloSurface,
    path_ptr: *mut VloPath,
    r: u8, g: u8, b: u8, a: u8,
    _width: f32,
    _cap: i32,
    _join: i32
) {
    if surf.is_null() || path_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let p = unsafe { &*path_ptr };
    
    surface.ctx.set_paint(Color::from_rgba8(r, g, b, a));
    surface.ctx.stroke_path(&p.path);
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
