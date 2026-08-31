// Raqote C FFI Bridge
// Blueprint Reference: backends/raqote.md

use raqote::{DrawTarget, SolidSource, Source, DrawOptions, PathBuilder, StrokeStyle, LineCap, LineJoin,
             Gradient, GradientStop, Spread, Color, Point};


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

pub struct RqtPathBuf {
    pub path: raqote::Path,
}

pub struct RqtSourceBuf {
    pub source: Source<'static>,
}

#[no_mangle]
pub extern "C" fn rqt_source_create_solid(r: u8, g: u8, b: u8, a: u8) -> *mut RqtSourceBuf {
    let src = Source::Solid(SolidSource::from_unpremultiplied_argb(a, r, g, b));
    Box::into_raw(Box::new(RqtSourceBuf { source: src }))
}

#[no_mangle]
pub extern "C" fn rqt_source_create_gradient(
    kind: i32,
    x0: f32, y0: f32, x1: f32, y1: f32,
    offsets: *const f32,
    colors: *const u32,
    nstops: i32,
) -> *mut RqtSourceBuf {
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
        stops.push(GradientStop {
            position: offsets_slice[i],
            color: Color::new(a_val, r_val, g_val, b_val),
        });
    }

    let gradient = Gradient { stops };
    let src = if kind == 0 {
        Source::new_linear_gradient(gradient, Point::new(x0, y0), Point::new(x1, y1), Spread::Pad)
    } else {
        Source::new_radial_gradient(gradient, Point::new(x0, y0), x1, Spread::Pad)
    };
    Box::into_raw(Box::new(RqtSourceBuf { source: src }))
}

#[no_mangle]
pub extern "C" fn rqt_source_destroy(ptr: *mut RqtSourceBuf) {
    if ptr.is_null() { return; }
    unsafe { drop(Box::from_raw(ptr)); }
}

#[no_mangle]
pub extern "C" fn rqt_draw_fill_with_source(
    surf: *mut RqtSurface,
    buf_ptr: *const RqtPathBuf,
    src_ptr: *const RqtSourceBuf,
    _fill_rule: i32
) {
    if surf.is_null() || buf_ptr.is_null() || src_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
    let src_buf = unsafe { &*src_ptr };
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.fill(&path_buf.path, &src_buf.source, &opts);
}

#[no_mangle]
pub extern "C" fn rqt_draw_stroke_with_source(
    surf: *mut RqtSurface,
    buf_ptr: *const RqtPathBuf,
    src_ptr: *const RqtSourceBuf,
    width: f32,
    cap: i32,
    join: i32,
    dashes: *const f32,
    ndash: i32,
    dash_phase: f32
) {
    if surf.is_null() || buf_ptr.is_null() || src_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
    let src_buf = unsafe { &*src_ptr };
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
    let dash_vec = if !dashes.is_null() && ndash > 0 {
        unsafe { std::slice::from_raw_parts(dashes, ndash as usize).to_vec() }
    } else {
        vec![]
    };
    let style = StrokeStyle {
        width,
        cap: line_cap,
        join: line_join,
        miter_limit: 4.0,
        dash_array: dash_vec,
        dash_offset: dash_phase,
    };
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.stroke(&path_buf.path, &src_buf.source, &style, &opts);
}

#[no_mangle]
pub extern "C" fn rqt_path_build(ptr: *mut RqtPath) -> *mut RqtPathBuf {
    if ptr.is_null() { return std::ptr::null_mut(); }
    let path_box = unsafe { Box::from_raw(ptr) };
    let finished_path = path_box.pb.finish();
    Box::into_raw(Box::new(RqtPathBuf { path: finished_path }))
}

#[no_mangle]
pub extern "C" fn rqt_path_buf_destroy(ptr: *mut RqtPathBuf) {
    if ptr.is_null() { return; }
    unsafe { drop(Box::from_raw(ptr)); }
}

#[no_mangle]
pub extern "C" fn rqt_draw_fill_path_buf(
    surf: *mut RqtSurface,
    buf_ptr: *const RqtPathBuf,
    r: u8, g: u8, b: u8, a: u8,
    _fill_rule: i32
) {
    if surf.is_null() || buf_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
    let src = Source::Solid(SolidSource::from_unpremultiplied_argb(a, r, g, b));
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.fill(&path_buf.path, &src, &opts);
}

#[no_mangle]
pub extern "C" fn rqt_draw_fill_path_buf_gradient(
    surf: *mut RqtSurface,
    buf_ptr: *const RqtPathBuf,
    kind: i32,
    x0: f32, y0: f32, x1: f32, y1: f32,
    offsets: *const f32,
    colors: *const u32,
    nstops: i32,
    _fill_rule: i32,
) {
    if surf.is_null() || buf_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
    if offsets.is_null() || colors.is_null() || nstops <= 0 { return; }
    let offsets_slice = unsafe { std::slice::from_raw_parts(offsets, nstops as usize) };
    let colors_slice = unsafe { std::slice::from_raw_parts(colors, nstops as usize) };

    let mut stops = Vec::with_capacity(nstops as usize);
    for i in 0..nstops as usize {
        let c = colors_slice[i];
        let r_val = (c & 0xFF) as u8;
        let g_val = ((c >> 8) & 0xFF) as u8;
        let b_val = ((c >> 16) & 0xFF) as u8;
        let a_val = ((c >> 24) & 0xFF) as u8;
        stops.push(GradientStop {
            position: offsets_slice[i],
            color: Color::new(a_val, r_val, g_val, b_val),
        });
    }

    let gradient = Gradient { stops };
    let src = if kind == 0 {
        Source::new_linear_gradient(gradient, Point::new(x0, y0), Point::new(x1, y1), Spread::Pad)
    } else {
        Source::new_radial_gradient(gradient, Point::new(x0, y0), x1, Spread::Pad)
    };

    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.fill(&path_buf.path, &src, &opts);
}

#[no_mangle]
pub extern "C" fn rqt_draw_stroke_path_buf(
    surf: *mut RqtSurface,
    buf_ptr: *const RqtPathBuf,
    r: u8, g: u8, b: u8, a: u8,
    width: f32,
    cap: i32,
    join: i32,
    dashes: *const f32,
    ndash: i32,
    dash_phase: f32
) {
    if surf.is_null() || buf_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
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
    let dash_vec = if !dashes.is_null() && ndash > 0 {
        unsafe { std::slice::from_raw_parts(dashes, ndash as usize).to_vec() }
    } else {
        vec![]
    };
    let style = StrokeStyle {
        width,
        cap: line_cap,
        join: line_join,
        miter_limit: 4.0,
        dash_array: dash_vec,
        dash_offset: dash_phase,
    };
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.stroke(&path_buf.path, &src, &style, &opts);
}

#[no_mangle]
pub extern "C" fn rqt_clip_push_buf(surf: *mut RqtSurface, buf_ptr: *const RqtPathBuf) {
    if surf.is_null() || buf_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_buf = unsafe { &*buf_ptr };
    surface.dt.push_clip(&path_buf.path);
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

/// Fill a path with a gradient.
/// kind: 0 = linear (x0,y0 -> x1,y1), 1 = radial (center x0,y0, radius x1).
/// colors: packed r | g<<8 | b<<16 | a<<24 per stop. The C++ caller
/// pre-swaps R/B so the ARGB32 output bytes land in contract (RGBA) order,
/// same convention as the solid-color entry points.
#[no_mangle]
pub extern "C" fn rqt_fill_path_gradient(
    surf: *mut RqtSurface,
    path_ptr: *mut RqtPath,
    kind: i32,
    x0: f32, y0: f32, x1: f32, y1: f32,
    offsets: *const f32,
    colors: *const u32,
    nstops: i32,
    _fill_rule: i32,
) {
    if surf.is_null() || path_ptr.is_null() || offsets.is_null() || colors.is_null() {
        return;
    }
    if nstops <= 0 {
        return;
    }
    let surface = unsafe { &mut *surf };
    let path_box = unsafe { Box::from_raw(path_ptr) };
    let finished_path = path_box.pb.finish();

    let n = nstops as usize;
    let offs = unsafe { std::slice::from_raw_parts(offsets, n) };
    let cols = unsafe { std::slice::from_raw_parts(colors, n) };
    let stops: Vec<GradientStop> = (0..n)
        .map(|i| {
            let c = cols[i];
            let (r, g, b, a) = (
                (c & 0xFF) as u8,
                ((c >> 8) & 0xFF) as u8,
                ((c >> 16) & 0xFF) as u8,
                ((c >> 24) & 0xFF) as u8,
            );
            GradientStop { position: offs[i], color: Color::new(a, r, g, b) }
        })
        .collect();

    let gradient = Gradient { stops };
    let src = if kind == 0 {
        Source::new_linear_gradient(gradient, Point::new(x0, y0), Point::new(x1, y1), Spread::Pad)
    } else {
        Source::new_radial_gradient(gradient, Point::new(x0, y0), x1, Spread::Pad)
    };
    let opts = DrawOptions {
        blend_mode: raqote::BlendMode::SrcOver,
        alpha: 1.0,
        antialias: raqote::AntialiasMode::Gray,
    };
    surface.dt.fill(&finished_path, &src, &opts);
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
    join: i32,
    dashes: *const f32,
    ndash: i32,
    dash_phase: f32
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
    
    let dash_vec = if !dashes.is_null() && ndash > 0 {
        unsafe { std::slice::from_raw_parts(dashes, ndash as usize).to_vec() }
    } else {
        vec![]
    };
    
    let style = StrokeStyle {
        width,
        cap: line_cap,
        join: line_join,
        miter_limit: 4.0,
        dash_array: dash_vec,
        dash_offset: dash_phase,
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

#[no_mangle]
pub extern "C" fn rqt_clip_push(surf: *mut RqtSurface, path_ptr: *mut RqtPath) {
    if surf.is_null() || path_ptr.is_null() { return; }
    let surface = unsafe { &mut *surf };
    let path_box = unsafe { Box::from_raw(path_ptr) };
    let finished_path = path_box.pb.finish();
    surface.dt.push_clip(&finished_path);
}

#[no_mangle]
pub extern "C" fn rqt_clip_pop(surf: *mut RqtSurface) {
    if surf.is_null() { return; }
    let surface = unsafe { &mut *surf };
    surface.dt.pop_clip();
}
