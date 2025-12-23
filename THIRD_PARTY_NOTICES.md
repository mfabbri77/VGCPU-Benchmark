# Third-Party Notices

This file contains the licenses and notices for third-party software used by VGCPU-Benchmark.

## Tier-1 Dependencies (Always Included)

### nlohmann/json
- **License**: MIT
- **Repository**: https://github.com/nlohmann/json
- **Copyright**: Copyright (c) 2013-2024 Niels Lohmann

```
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### PlutoVG
- **License**: MIT
- **Repository**: https://github.com/nickhutchinson/pluto
- **Copyright**: Copyright (c) 2023 nickhutchinson

```
MIT License
(Same terms as above)
```

### Blend2D
- **License**: Zlib
- **Repository**: https://github.com/blend2d/blend2d
- **Copyright**: Copyright (c) 2017-2024 The Blend2D Authors

```
Zlib License

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### AsmJit
- **License**: Zlib
- **Repository**: https://github.com/asmjit/asmjit
- **Copyright**: Copyright (c) 2008-2024 The AsmJit Authors

```
(Same Zlib terms as Blend2D)
```

## Optional Dependencies

### Cairo (when ENABLE_CAIRO=ON)
- **License**: LGPL-2.1 or MPL-1.1
- **Repository**: https://www.cairographics.org/

### Skia (when ENABLE_SKIA=ON)
- **License**: BSD-3-Clause
- **Repository**: https://skia.org/
- **Copyright**: Copyright (c) 2024 Google LLC

### ThorVG (when ENABLE_THORVG=ON)
- **License**: MIT
- **Repository**: https://github.com/thorvg/thorvg
- **Copyright**: Copyright (c) 2020-2024 The ThorVG Project

### AGG (Anti-Grain Geometry) (when ENABLE_AGG=ON)
- **License**: BSD-3-Clause
- **Repository**: https://github.com/ghaerr/agg-2.6
- **Copyright**: Copyright (c) 2002-2006 Maxim Shemanarev

### FreeType (when ENABLE_AGG=ON)
- **License**: FreeType License (BSD-style) or GPL-2.0+
- **Repository**: https://freetype.org/

### Qt (when ENABLE_QT=ON)
- **License**: LGPL-3.0 or Commercial
- **Repository**: https://www.qt.io/

### AmanithVG (when ENABLE_AMANITHVG=ON)
- **License**: Proprietary (Free for non-commercial use)
- **Repository**: https://www.amanithvg.com/

### Raqote (when ENABLE_RAQOTE=ON)
- **License**: MIT or Apache-2.0
- **Repository**: https://github.com/nickhutchinson/raqote

### vello_cpu (when ENABLE_VELLO_CPU=ON)
- **License**: MIT or Apache-2.0
- **Repository**: https://github.com/nickhutchinson/vello

## Testing Dependencies

### doctest
- **License**: MIT
- **Repository**: https://github.com/doctest/doctest
- **Copyright**: Copyright (c) 2016-2024 Viktor Kirilov

```
MIT License
(Same terms as nlohmann/json)
```

## Build Tools

### Corrosion (when Rust backends enabled)
- **License**: MIT
- **Repository**: https://github.com/corrosion-rs/corrosion

---

For the full text of each license, please refer to the respective project repositories.
