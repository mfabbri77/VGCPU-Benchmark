// Raqote C FFI Bridge
// Blueprint Reference: backends/raqote.md

use raqote::{DrawTarget, SolidSource, Source, DrawOptions, PathBuilder, StrokeStyle, LineCap, LineJoin};


/// Opaque handle to Raqote DrawTarget
pub struct RqtSurface {
    dt: DrawTarget,
    width: i32,
    height: i32,
}

/// Path builder state (for incremental path construction)
pub struct RqtPath {
    pb: PathBuilder,
}

// ============================================================================
// Surface Management
// ============================================================================

#[no_mangle]
pub extern "C" fn rqt_create(width: i32, height: i32) -> *mut RqtSurface {
    let dt = DrawTarget::new(width, height);
    Box::into_raw(Box::new(RqtSurface { dt, width, height }))
}

#[no_mangle]
pub extern "C" fn rqt_destroy(ptr: *mut RqtSurface) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn rqt_clear(ptr: *mut RqtSurface, r: u8, g: u8, b: u8, a: u8) {
    if ptr.is_null() { return; }
    let surf = unsafe { &mut *ptr };
    let color = SolidSource::from_unpremultiplied_argb(a, r, g, b);
    surf.dt.clear(color);
}

#[no_mangle]
pub extern "C" fn rqt_get_pixels(ptr: *mut RqtSurface, out_buf: *mut u32) {
    if ptr.is_null() || out_buf.is_null() { return; }
    let surf = unsafe { &mut *ptr };
    let data = surf.dt.get_data();
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), out_buf, data.len());
    }
}

#[no_mangle]
pub extern "C" fn rqt_get_width(ptr: *mut RqtSurface) -> i32 {
    if ptr.is_null() { return 0; }
    let surf = unsafe { &*ptr };
    surf.width
}

#[no_mangle]
pub extern "C" fn rqt_get_height(ptr: *mut RqtSurface) -> i32 {
    if ptr.is_null() { return 0; }
    let surf = unsafe { &*ptr };
    surf.height
}

// ============================================================================
// Path Construction
// ============================================================================

#[no_mangle]
pub extern "C" fn rqt_path_create() -> *mut RqtPath {
    Box::into_raw(Box::new(RqtPath { pb: PathBuilder::new() }))
}

#[no_mangle]
pub extern "C" fn rqt_path_destroy(ptr: *mut RqtPath) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn rqt_path_move_to(ptr: *mut RqtPath, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.move_to(x, y);
}

#[no_mangle]
pub extern "C" fn rqt_path_line_to(ptr: *mut RqtPath, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.line_to(x, y);
}

#[no_mangle]
pub extern "C" fn rqt_path_quad_to(ptr: *mut RqtPath, cx: f32, cy: f32, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.quad_to(cx, cy, x, y);
}

#[no_mangle]
pub extern "C" fn rqt_path_cubic_to(ptr: *mut RqtPath, c1x: f32, c1y: f32, c2x: f32, c2y: f32, x: f32, y: f32) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.cubic_to(c1x, c1y, c2x, c2y, x, y);
}

#[no_mangle]
pub extern "C" fn rqt_path_close(ptr: *mut RqtPath) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.close();
}

#[no_mangle]
pub extern "C" fn rqt_path_rect(ptr: *mut RqtPath, x: f32, y: f32, w: f32, h: f32) {
    if ptr.is_null() { return; }
    let path = unsafe { &mut *ptr };
    path.pb.rect(x, y, w, h);
}

// ============================================================================
// Drawing Operations
// ============================================================================

/// Fill a path with solid color
/// fill_rule: 0 = NonZero, 1 = EvenOdd
#[no_mangle]
pub extern "C" fn rqt_fill_path(
    surf: *mut RqtSurface,
    path_ptr: *mut RqtPath,
    r: u8, g: u8, b: u8, a: u8,
    _fill_rule: i32
) {
    if surf.is_null() || path_ptr.is_null() { return; }
    
    let surface = unsafe { &mut *surf };
    let path_box = unsafe { Box::from_raw(path_ptr) };
    let finished_path = path_box.pb.finish();
    
    let src = Source::Solid(SolidSource::from_unpremultiplied_argb(a, r, g, b));
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    
    surface.dt.fill(&finished_path, &src, &opts);
    
    // Recreate the path for potential reuse (consume the finished path)
    // Note: In practice, the C++ side should create new paths each time
}

/// Stroke a path with solid color
/// cap: 0 = Butt, 1 = Round, 2 = Square
/// join: 0 = Miter, 1 = Round, 2 = Bevel
#[no_mangle]
pub extern "C" fn rqt_stroke_path(
    surf: *mut RqtSurface,
    path_ptr: *mut RqtPath,
    r: u8, g: u8, b: u8, a: u8,
    width: f32,
    cap: i32,
    join: i32
) {
    if surf.is_null() || path_ptr.is_null() { return; }
    
    let surface = unsafe { &mut *surf };
    let path_box = unsafe { Box::from_raw(path_ptr) };
    let finished_path = path_box.pb.finish();
    
    let src = Source::Solid(SolidSource::from_unpremultiplied_argb(a, r, g, b));
    
    let line_cap = match cap {
        1 => LineCap::Round,
        2 => LineCap::Square,
        _ => LineCap::Butt,
    };
    
    let line_join = match join {
        1 => LineJoin::Round,
        2 => LineJoin::Bevel,
        _ => LineJoin::Miter,
    };
    
    let style = StrokeStyle {
        width,
        cap: line_cap,
        join: line_join,
        miter_limit: 4.0,
        dash_array: vec![],
        dash_offset: 0.0,
    };
    
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    
    surface.dt.stroke(&finished_path, &src, &style, &opts);
}

/// Simple rectangle fill (convenience function)
#[no_mangle]
pub extern "C" fn rqt_fill_rect(
    surf: *mut RqtSurface,
    x: f32, y: f32, w: f32, h: f32,
    r: u8, g: u8, b: u8, a: u8
) {
    if surf.is_null() { return; }
    
    let surface = unsafe { &mut *surf };
    let mut pb = PathBuilder::new();
    pb.rect(x, y, w, h);
    let path = pb.finish();
    
    let src = Source::Solid(SolidSource::from_unpremultiplied_argb(a, r, g, b));
    let opts = DrawOptions::default();
    
    surface.dt.fill(&path, &src, &opts);
}
