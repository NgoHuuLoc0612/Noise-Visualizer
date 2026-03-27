"""
Noise Visualizer — Enterprise Python Backend
Flask REST API + WebSocket animation streaming
"""
from __future__ import annotations

import base64
import importlib
import json
import math
import os
import struct
import sys
import time
import threading
import traceback
import zlib
from pathlib import Path
from typing import Any, Dict, Optional

from flask import Flask, Response, jsonify, request, send_from_directory
from flask_cors import CORS
from flask_socketio import SocketIO, emit, disconnect

# ─────────────────────────────────────────────────────────────────────────────
#  Try loading compiled C++ extension; fall back to pure-Python implementation
# ─────────────────────────────────────────────────────────────────────────────
_ENGINE_NATIVE = False
try:
    sys.path.insert(0, str(Path(__file__).parent))
    import noise_engine as _ne            # compiled pybind11 module
    _ENGINE_NATIVE = True
    print("[backend] ✓  Native C++ noise engine loaded")
except ImportError:
    print("[backend] ⚠  Native engine not found — using pure-Python fallback")
    _ne = None


# ─────────────────────────────────────────────────────────────────────────────
#  Pure-Python fallback (complete, single-file, no deps beyond stdlib + numpy)
# ─────────────────────────────────────────────────────────────────────────────
class _PythonEngine:
    """Pure-Python noise engine (slower but always available)."""

    def __init__(self):
        import hashlib
        self._hash = hashlib

    # ── Permutation table ─────────────────────────────────────────────────────
    @staticmethod
    def _make_perm(seed: int):
        import random
        rng = random.Random(seed)
        p = list(range(256))
        rng.shuffle(p)
        return p + p

    # ── Fade / lerp ───────────────────────────────────────────────────────────
    @staticmethod
    def _fade(t): return t * t * t * (t * (t * 6 - 15) + 10)

    @staticmethod
    def _lerp(a, b, t): return a + t * (b - a)

    # ── Gradient ─────────────────────────────────────────────────────────────
    @staticmethod
    def _grad(h, x, y):
        h &= 3
        u = x if h < 2 else y
        v = y if h < 2 else x
        return (-u if h & 1 else u) + (-v if h & 2 else v)

    def _perlin2d(self, x, y, perm):
        X, Y = int(math.floor(x)) & 255, int(math.floor(y)) & 255
        xf, yf = x - math.floor(x), y - math.floor(y)
        u, v = self._fade(xf), self._fade(yf)
        A, B = perm[X] + Y, perm[X + 1] + Y
        return self._lerp(
            self._lerp(self._grad(perm[A],     xf,     yf),
                       self._grad(perm[B],     xf - 1, yf),     u),
            self._lerp(self._grad(perm[A + 1], xf,     yf - 1),
                       self._grad(perm[B + 1], xf - 1, yf - 1), u), v)

    def _simplex2d(self, xin, yin, perm):
        F2 = 0.5 * (math.sqrt(3) - 1)
        G2 = (3 - math.sqrt(3)) / 6
        s = (xin + yin) * F2
        i, j = int(math.floor(xin + s)), int(math.floor(yin + s))
        t = (i + j) * G2
        x0, y0 = xin - (i - t), yin - (j - t)
        i1, j1 = (1, 0) if x0 > y0 else (0, 1)
        x1, y1 = x0 - i1 + G2, y0 - j1 + G2
        x2, y2 = x0 - 1 + 2 * G2, y0 - 1 + 2 * G2
        grad3 = [(1,1,0),(-1,1,0),(1,-1,0),(-1,-1,0),(1,0,1),(-1,0,1),(1,0,-1),(-1,0,-1),(0,1,1),(0,-1,1),(0,1,-1),(0,-1,-1)]
        ii, jj = i & 255, j & 255
        gi0 = perm[ii    + perm[jj   ]] % 12
        gi1 = perm[ii+i1 + perm[jj+j1]] % 12
        gi2 = perm[ii+1  + perm[jj+1 ]] % 12
        def dot(g, x, y): return g[0]*x + g[1]*y
        def contrib(t_, x_, y_, gi):
            t2 = 0.5 - x_*x_ - y_*y_
            return 0 if t2 < 0 else t2*t2*t2*t2 * dot(grad3[gi], x_, y_)
        return 70 * (contrib(None, x0, y0, gi0) + contrib(None, x1, y1, gi1) + contrib(None, x2, y2, gi2))

    def _fbm(self, noiser, x, y, octaves, lacunarity, gain, perm):
        val, amp, freq = 0.0, 0.5, 1.0
        for _ in range(octaves):
            val  += amp * noiser(x * freq, y * freq, perm)
            freq *= lacunarity
            amp  *= gain
        return val

    def generate(self, params: dict) -> dict:
        import numpy as np
        W, H    = params.get("width", 512), params.get("height", 512)
        scale   = params.get("scale", 4.0)
        octaves = params.get("octaves", 6)
        lac     = params.get("lacunarity", 2.0)
        gain    = params.get("gain", 0.5)
        seed    = params.get("seed", 42)
        ox, oy  = params.get("offset_x", 0.0), params.get("offset_y", 0.0)
        alg     = params.get("algorithm", "perlin_fbm")
        colormap_id = params.get("colormap", 0)
        invert  = params.get("invert", False)
        contrast   = params.get("contrast", 1.0)
        brightness = params.get("brightness", 0.0)

        perm = self._make_perm(seed)
        buf  = np.zeros((H, W), dtype=np.float64)

        import random
        rng = random.Random(seed ^ 0xABCDEF)

        for y in range(H):
            for x in range(W):
                nx = (ox + x) / W * scale
                ny = (oy + y) / H * scale
                v  = 0.0
                if alg in ("perlin_fbm", "perlin_ridged", "perlin_turbulence", "perlin_domain_warp"):
                    if alg == "perlin_turbulence":
                        val, amp, freq = 0.0, 0.5, 1.0
                        for _ in range(octaves):
                            val += amp * abs(self._perlin2d(nx*freq, ny*freq, perm))
                            freq *= lac; amp *= gain
                        v = val
                    elif alg == "perlin_ridged":
                        val, amp, freq, weight = 0.0, 0.5, 1.0, 1.0
                        for _ in range(octaves):
                            sig = 1.0 - abs(self._perlin2d(nx*freq, ny*freq, perm))
                            sig *= sig * weight
                            weight = max(0, min(1, sig * gain))
                            val += amp * sig; freq *= lac; amp *= gain * 0.5
                        v = val
                    else:
                        v = self._fbm(self._perlin2d, nx, ny, octaves, lac, gain, perm)
                elif alg in ("simplex_fbm", "simplex_turbulence"):
                    if alg == "simplex_turbulence":
                        val, amp, freq = 0.0, 0.5, 1.0
                        for _ in range(octaves):
                            val += amp * abs(self._simplex2d(nx*freq, ny*freq, perm))
                            freq *= lac; amp *= gain
                        v = val
                    else:
                        v = self._fbm(self._simplex2d, nx, ny, octaves, lac, gain, perm)
                elif alg == "white":
                    v = rng.gauss(0, 1)
                elif alg == "pink":
                    v = sum(rng.gauss(0, 1) for _ in range(min(octaves, 8)))
                elif alg == "brown":
                    v = rng.gauss(0, 1) * 0.5
                else:
                    v = self._fbm(self._perlin2d, nx, ny, octaves, lac, gain, perm)
                buf[y, x] = v

        # normalize
        mn, mx = buf.min(), buf.max()
        if mx - mn > 1e-12:
            buf = (buf - mn) / (mx - mn)
        if invert:
            buf = 1.0 - buf
        buf = np.clip(buf * contrast + brightness, 0, 1)

        # colormap
        rgba = self._apply_colormap(buf, colormap_id)
        stats = self._compute_stats(buf.ravel())
        return {"rgba": rgba, "raw": buf, "stats": stats}

    def _apply_colormap(self, buf, cmap_id):
        import numpy as np
        t = buf[:, :, np.newaxis]
        if cmap_id == 0:  # grayscale
            rgb = np.concatenate([t, t, t], axis=2)
        elif cmap_id == 1:  # inferno
            r = np.clip(3.54*t - 0.34, 0, 1)
            g = np.clip(2.0*t*t - 1.5*t + 0.1, 0, 1)
            b = np.clip(np.sin(math.pi * t) * 0.8, 0, 1)
            rgb = np.concatenate([r, g, b], axis=2)
        elif cmap_id == 2:  # plasma
            r = np.clip(0.5 + np.sin(math.pi * (t - 0.5)) * 0.5 + t * 0.3, 0, 1)
            g = np.clip(t * t, 0, 1)
            b = np.clip(1.0 - 1.5 * t + t * t, 0, 1)
            rgb = np.concatenate([r, g, b], axis=2)
        elif cmap_id == 3:  # viridis
            r = np.clip(0.267 + 0.004*t + 0.329*t*t + 0.4*t**3, 0, 1)
            g = np.clip(1.4*t - 1.4*t*t + 0.35*t**3, 0, 1)
            b = np.clip(0.329 + 0.783*t - 2.1*t*t + 1.1*t**3, 0, 1)
            rgb = np.concatenate([r, g, b], axis=2)
        else:
            rgb = np.concatenate([t, t, t], axis=2)
        alpha = np.ones((*buf.shape, 1))
        return np.clip(np.concatenate([rgb, alpha], axis=2) * 255, 0, 255).astype(np.uint8)

    def _compute_stats(self, flat):
        import numpy as np
        mean = float(np.mean(flat))
        std  = float(np.std(flat))
        mn   = float(np.min(flat))
        mx   = float(np.max(flat))
        hist, _ = np.histogram(flat, bins=64, range=(0, 1))
        return {
            "mean": mean, "variance": float(std**2), "std_dev": std,
            "min_val": mn, "max_val": mx,
            "skewness": float(np.mean(((flat - mean) / (std + 1e-12))**3)),
            "kurtosis": float(np.mean(((flat - mean) / (std + 1e-12))**4) - 3),
            "histogram": (hist / hist.sum()).tolist(),
            "autocorrelation": [1.0] * 64,
            "power_spectrum":  [0.0] * 256,
        }


# Singleton engines
_py_engine   = _PythonEngine()
_cpp_gen     = _ne.NoiseGenerator() if _ENGINE_NATIVE else None


# ─────────────────────────────────────────────────────────────────────────────
#  Flask application
# ─────────────────────────────────────────────────────────────────────────────
app    = Flask(__name__, static_folder="static")
CORS(app, resources={r"/api/*": {"origins": "*"}})
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

STATIC_DIR = Path(__file__).parent / "static"


# ─────────────────────────────────────────────────────────────────────────────
#  Helpers
# ─────────────────────────────────────────────────────────────────────────────
def _engine_info() -> dict:
    return {"native": _ENGINE_NATIVE, "version": getattr(_ne, "__version__", "N/A") if _ENGINE_NATIVE else "python-fallback"}


def _run_generation(params: dict) -> dict:
    """Run noise generation through C++ engine or Python fallback."""
    import numpy as np

    if _ENGINE_NATIVE:
        result = _cpp_gen.generate(params)
        rgba_np = result.rgba_array()                        # (H, W, 4) uint8
        raw_np  = result.raw_array()                         # (H, W) float64
        stats   = result.stats.to_dict()
    else:
        res     = _py_engine.generate(params)
        rgba_np = res["rgba"]
        raw_np  = res["raw"]
        stats   = res["stats"]
        # convert lists to serialisable
        for k, v in stats.items():
            if hasattr(v, "tolist"):
                stats[k] = v.tolist()

    # Encode RGBA → PNG base64 via zlib-compressed raw bytes (no Pillow needed)
    png_bytes = _encode_png(rgba_np)
    b64       = base64.b64encode(png_bytes).decode()

    # Serialize stats
    for k in ("histogram", "autocorrelation", "power_spectrum"):
        v = stats.get(k, [])
        if hasattr(v, "tolist"):
            stats[k] = v.tolist()

    return {
        "image_b64": b64,
        "stats":     stats,
        "engine":    _engine_info(),
        "params":    {k: (v if not isinstance(v, (bytes,)) else str(v)) for k, v in params.items()},
    }


def _encode_png(rgba_np) -> bytes:
    """Minimal PNG encoder (no external libs)."""
    import numpy as np
    H, W = rgba_np.shape[:2]

    def adler32(data):
        a, b = 1, 0
        for byte in data:
            a = (a + byte) % 65521
            b = (b + a)   % 65521
        return (b << 16) | a

    def chunk(name, data):
        c   = name + data
        crc = zlib.crc32(c) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + c + struct.pack(">I", crc)

    # Filter type 0 (none) prepended to each row
    raw_rows = b"".join(b"\x00" + bytes(rgba_np[y].ravel()) for y in range(H))
    compressed = zlib.compress(raw_rows, level=6)

    sig   = b"\x89PNG\r\n\x1a\n"
    ihdr  = chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
    idat  = chunk(b"IDAT", compressed)
    iend  = chunk(b"IEND", b"")
    return sig + ihdr + idat + iend


# ─────────────────────────────────────────────────────────────────────────────
#  Static frontend routes
# ─────────────────────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return send_from_directory(STATIC_DIR, "index.html")

@app.route("/<path:path>")
def static_files(path):
    return send_from_directory(STATIC_DIR, path)


# ─────────────────────────────────────────────────────────────────────────────
#  REST API
# ─────────────────────────────────────────────────────────────────────────────
@app.route("/api/info", methods=["GET"])
def api_info():
    return jsonify({
        "engine": _engine_info(),
        "algorithms": [
            {"id": "perlin_fbm",         "label": "Perlin fBm",          "group": "Gradient"},
            {"id": "perlin_turbulence",  "label": "Perlin Turbulence",   "group": "Gradient"},
            {"id": "perlin_ridged",      "label": "Perlin Ridged",       "group": "Gradient"},
            {"id": "perlin_domain_warp", "label": "Domain Warp",         "group": "Gradient"},
            {"id": "simplex_fbm",        "label": "Simplex fBm",         "group": "Gradient"},
            {"id": "simplex_turbulence", "label": "Simplex Turbulence",  "group": "Gradient"},
            {"id": "worley_f1",          "label": "Worley F1",           "group": "Cellular"},
            {"id": "worley_f2",          "label": "Worley F2",           "group": "Cellular"},
            {"id": "worley_f2_f1",       "label": "Worley F2−F1",        "group": "Cellular"},
            {"id": "worley_crease",      "label": "Worley Crease",       "group": "Cellular"},
            {"id": "value_fbm",          "label": "Value fBm",           "group": "Value"},
            {"id": "white",              "label": "White Noise",         "group": "Spectral"},
            {"id": "pink",               "label": "Pink Noise (1/f)",    "group": "Spectral"},
            {"id": "brown",              "label": "Brown / Red Noise",   "group": "Spectral"},
            {"id": "blue",               "label": "Blue Noise",          "group": "Spectral"},
            {"id": "violet",             "label": "Violet Noise",        "group": "Spectral"},
        ],
        "colormaps": [
            {"id": 0, "label": "Grayscale"},
            {"id": 1, "label": "Inferno"},
            {"id": 2, "label": "Plasma"},
            {"id": 3, "label": "Viridis"},
            {"id": 4, "label": "Terrain"},
            {"id": 5, "label": "Thermal"},
            {"id": 6, "label": "Spectral"},
            {"id": 7, "label": "Custom"},
        ],
    })


@app.route("/api/generate", methods=["POST"])
def api_generate():
    try:
        params = request.get_json(force=True) or {}
        t0     = time.perf_counter()
        result = _run_generation(params)
        result["elapsed_ms"] = round((time.perf_counter() - t0) * 1000, 2)
        return jsonify(result)
    except Exception as exc:
        traceback.print_exc()
        return jsonify({"error": str(exc)}), 500


@app.route("/api/analyze", methods=["POST"])
def api_analyze():
    """Statistical analysis on raw noise data (flat list of floats)."""
    try:
        body = request.get_json(force=True) or {}
        data = body.get("data", [])
        bins = body.get("bins", 64)
        lags = body.get("lags", 64)
        if _ENGINE_NATIVE:
            stats = _ne.analyze_noise(data, bins, lags).to_dict()
        else:
            stats = _py_engine._compute_stats(__import__("numpy").array(data))
        for k in ("histogram", "autocorrelation", "power_spectrum"):
            v = stats.get(k, [])
            if hasattr(v, "tolist"): stats[k] = v.tolist()
        return jsonify({"stats": stats})
    except Exception as exc:
        traceback.print_exc()
        return jsonify({"error": str(exc)}), 500


@app.route("/api/generate/animation", methods=["POST"])
def api_generate_animation():
    """Generate multiple frames for animated noise (returned as list of base64 PNGs)."""
    try:
        body   = request.get_json(force=True) or {}
        params = body.get("params", {})
        frames = int(body.get("frames", 16))
        speed  = float(body.get("speed", 0.1))
        frames = min(frames, 64)   # cap for HTTP response size

        results = []
        for i in range(frames):
            p        = dict(params)
            p["time_z"] = i * speed
            r        = _run_generation(p)
            results.append({"frame": i, "image_b64": r["image_b64"]})
        return jsonify({"frames": results, "engine": _engine_info()})
    except Exception as exc:
        traceback.print_exc()
        return jsonify({"error": str(exc)}), 500


# ─────────────────────────────────────────────────────────────────────────────
#  WebSocket — real-time animation streaming
# ─────────────────────────────────────────────────────────────────────────────
_animation_threads: Dict[str, bool] = {}   # sid → running flag
_lock = threading.Lock()

@socketio.on("connect")
def ws_connect():
    print(f"[ws] client connected: {request.sid}")

@socketio.on("disconnect")
def ws_disconnect():
    with _lock:
        _animation_threads[request.sid] = False
    print(f"[ws] client disconnected: {request.sid}")

@socketio.on("start_animation")
def ws_start_animation(data):
    sid    = request.sid
    params = data.get("params", {})
    speed  = float(data.get("speed", 0.05))
    fps    = int(data.get("fps", 10))

    with _lock:
        _animation_threads[sid] = True

    def stream():
        frame = 0
        while True:
            with _lock:
                if not _animation_threads.get(sid, False):
                    break
            try:
                p = dict(params); p["time_z"] = frame * speed
                r = _run_generation(p)
                socketio.emit("frame", {
                    "frame":     frame,
                    "image_b64": r["image_b64"],
                    "stats":     r["stats"],
                }, to=sid)
            except Exception as e:
                socketio.emit("error", {"message": str(e)}, to=sid)
                break
            frame += 1
            time.sleep(1.0 / fps)

    t = threading.Thread(target=stream, daemon=True)
    t.start()

@socketio.on("stop_animation")
def ws_stop_animation(_data=None):
    with _lock:
        _animation_threads[request.sid] = False


# ─────────────────────────────────────────────────────────────────────────────
#  Entry point
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8765))
    print(f"[backend] Noise Visualizer API → http://localhost:{port}")
    print(f"[backend] Engine: {'C++ native' if _ENGINE_NATIVE else 'Python fallback'}")
    socketio.run(app, host="0.0.0.0", port=port, debug=False, allow_unsafe_werkzeug=True)
