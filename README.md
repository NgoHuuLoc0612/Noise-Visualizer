# NOISE VISUALIZER

> Real-time procedural noise generation, visualization, and statistical analysis.
> Python + C++ (pybind11) backend · Vanilla JS + WebGL frontend.

---

## Architecture

```
noise_visualizer/
├── noise_engine.hpp     C++ noise algorithms (header-only)
├── bindings.cpp         pybind11 Python↔C++ bridge
├── CMakeLists.txt       CMake build system
├── server.py            Flask REST API + Socket.IO streaming
├── requirements.txt     Python dependencies
├── setup.py             One-shot build script
└── static/
    ├── index.html       Application shell
    ├── style.css        Industrial dark-mode UI
    └── app.js           Frontend engine (rendering, charts, 3D, WebSocket)
```

---

## Algorithms

### Gradient Noise
| Algorithm | Description |
|---|---|
| **Perlin fBm** | Classic fractal Brownian motion using Ken Perlin's improved gradient noise |
| **Perlin Turbulence** | Absolute-value fBm creating sharp rock/cloud features |
| **Perlin Ridged** | Musgrave's ridged multi-fractal: weight-controlled inversion |
| **Domain Warp** | Íñigo Quílez's technique: fbm(p + k·fbm(p)) for swirling organics |
| **Simplex fBm** | OpenSimplex-class: reduced artifacts, O(n²) vs O(2ⁿ) |
| **Simplex Turbulence** | Simplex with absolute-value octave stacking |

### Cellular Noise
| Algorithm | Description |
|---|---|
| **Worley F1** | Distance to nearest feature point (Voronoi cells) |
| **Worley F2** | Distance to second-nearest (bubbly transitions) |
| **Worley F2−F1** | Cell-edge isolation (thin veins, cracks) |
| **Worley Crease** | \|F1+F2\| crystalline formations |

### Value Noise
| Algorithm | Description |
|---|---|
| **Value fBm** | Lattice-interpolated random values with quintic smoothstep |

### Spectral / Stochastic
| Algorithm | Description |
|---|---|
| **White** | i.i.d. Gaussian, flat PSD S(f) = σ² |
| **Pink (1/f)** | Voss-McCartney algorithm, S(f) ∝ f⁻¹ |
| **Brown/Red** | Integrated white noise, S(f) ∝ f⁻² |
| **Blue** | Differentiated pink, S(f) ∝ f |
| **Violet** | Differentiated white, S(f) ∝ f² |

---

## Visualizations

- **2D Field** — Raw noise texture with zoom/pan, pixel-level crosshair readout
- **3D Surface** — WebGL mesh with drag-to-rotate, inferno colormap, perspective projection
- **Histogram** — Empirical probability density function
- **Autocorrelation** — ACF with configurable lag count
- **Power Spectrum** — Log-scale PSD via DFT
- **Waveform** — Scanline slice with row selector

---

## Statistical Analysis

Per-generation statistics: mean, variance, std deviation, min/max, skewness (3rd central moment), excess kurtosis (4th central moment − 3).

---

## Colormaps

Grayscale · Inferno · Plasma · Viridis · Terrain · Thermal · Spectral · Custom (user-defined gradient)

---

## Setup

### Prerequisites
- Python ≥ 3.11
- CMake ≥ 3.18
- C++17 compiler (GCC ≥ 11 / Clang ≥ 14 / MSVC 2022)
- pybind11 (auto-fetched by CMake if not found)

### Install & Run

```bash
# 1. Build C++ extension (optional but recommended for performance)
chmod +x build.sh
./build.sh

# 2. (Or) Install only Python deps for pure-Python fallback
pip install -r requirements.txt

# 3. Start the server
python3 server.py

# 4. Open browser
open http://localhost:8765
```

---

## API Reference

### `POST /api/generate`
```json
{
  "algorithm": "perlin_fbm",
  "width": 512, "height": 512,
  "scale": 4.0, "octaves": 6,
  "lacunarity": 2.0, "gain": 0.5,
  "warp_strength": 1.5,
  "seed": 42,
  "colormap": 0,
  "invert": false,
  "contrast": 1.0, "brightness": 0.0
}
```
Returns `{ image_b64, stats, elapsed_ms, engine }`.

### `GET /api/info`
Engine capabilities and algorithm/colormap catalogue.

### `POST /api/analyze`
Statistical analysis on an arbitrary `{ data: [float…] }` payload.

### `POST /api/generate/animation`
Batch-generate animation frames: `{ params, frames, speed }`.

### WebSocket Events
| Client → Server | Description |
|---|---|
| `start_animation` | Begin streaming frames `{ params, speed, fps }` |
| `stop_animation`  | Halt streaming |

| Server → Client | Description |
|---|---|
| `frame` | `{ frame, image_b64, stats }` |
| `error` | `{ message }` |

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `G` | Generate |
| `Esc` | Stop animation |
| `Alt+drag` | Pan canvas |
| Mouse wheel | Zoom canvas |

---

## Performance

With the native C++ engine (pybind11), a 512×512 Perlin fBm with 8 octaves generates in **~25–80 ms** on modern hardware. The pure-Python fallback is ~10–40× slower.

The 3D surface renderer uses WebGL2 with a configurable mesh subdivision (32×32 default) and supports real-time rotation via mouse drag.
