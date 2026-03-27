/* ─────────────────────────────────────────────────────────────────────────
   NOISE VISUALIZER — Enterprise Frontend Engine
   Single-file JS: State Management, API Layer, Canvas Renderer,
   3D Surface Renderer, Chart Engine, WebSocket Animation, Export
   ───────────────────────────────────────────────────────────────────────── */

"use strict";

// ═════════════════════════════════════════════════════════════════════════════
//  Configuration
// ═════════════════════════════════════════════════════════════════════════════
const API_BASE   = "http://localhost:8765";
const WS_URL     = "http://localhost:8765";

const ALGORITHMS = [
  { id: "perlin_fbm",         label: "Perlin fBm",        group: "Gradient" },
  { id: "perlin_turbulence",  label: "Turbulence",        group: "Gradient" },
  { id: "perlin_ridged",      label: "Ridged",            group: "Gradient" },
  { id: "perlin_domain_warp", label: "Domain Warp",       group: "Gradient" },
  { id: "simplex_fbm",        label: "Simplex fBm",       group: "Gradient" },
  { id: "simplex_turbulence", label: "Simplex Turb.",     group: "Gradient" },
  { id: "worley_f1",          label: "Worley F1",         group: "Cellular" },
  { id: "worley_f2",          label: "Worley F2",         group: "Cellular" },
  { id: "worley_f2_f1",       label: "Worley F2−F1",      group: "Cellular" },
  { id: "worley_crease",      label: "Crease",            group: "Cellular" },
  { id: "value_fbm",          label: "Value fBm",         group: "Value"    },
  { id: "white",              label: "White",             group: "Spectral" },
  { id: "pink",               label: "Pink 1/f",          group: "Spectral" },
  { id: "brown",              label: "Brown",             group: "Spectral" },
  { id: "blue",               label: "Blue",              group: "Spectral" },
  { id: "violet",             label: "Violet",            group: "Spectral" },
];

const COLORMAPS = [
  { id: 0, label: "Gray",     stops: ["#000","#fff"] },
  { id: 1, label: "Inferno",  stops: ["#000","#6a0a0a","#ff6600","#ffff00","#fff"] },
  { id: 2, label: "Plasma",   stops: ["#0d0221","#7a0d77","#e66000","#fbce00"] },
  { id: 3, label: "Viridis",  stops: ["#440154","#2d7f8e","#71cf57","#fde725"] },
  { id: 4, label: "Terrain",  stops: ["#0064b4","#228B22","#8B6914","#ffffff"] },
  { id: 5, label: "Thermal",  stops: ["#0000ff","#00ffff","#00ff00","#ffff00","#ff0000"] },
  { id: 6, label: "Spectral", stops: ["#9e0142","#3288bd","#66c2a5","#fee08b","#d53e4f"] },
  { id: 7, label: "Custom",   stops: ["#000","#fff"] },
];

const THEORY = {
  perlin_fbm: {
    title: "Perlin fBm",
    body: "Fractal Brownian Motion sums Perlin octaves with decreasing amplitude. Each layer multiplies frequency by lacunarity and amplitude by gain (Hurst exponent H = −log(gain)/log(lacunarity)).",
    formula: "f(x) = Σ gain^i · perlin(x · lac^i)"
  },
  perlin_turbulence: {
    title: "Perlin Turbulence",
    body: "Turbulence takes the absolute value of Perlin noise before summing octaves, creating sharp ridge-like features reminiscent of geological formations.",
    formula: "f(x) = Σ gain^i · |perlin(x · lac^i)|"
  },
  perlin_ridged: {
    title: "Ridged Multi-fractal",
    body: "Ridged noise inverts absolute value noise then squares it, controlled by a weight term that creates self-similar ridged mountain structures.",
    formula: "signal = (1 − |perlin|)² · weight"
  },
  perlin_domain_warp: {
    title: "Domain Warping",
    body: "Iq's technique: the input domain is displaced by the output of fBm before sampling, creating swirling organic patterns. Applied recursively for maximum complexity.",
    formula: "f(p) = fbm(p + k·fbm(p + k·fbm(p)))"
  },
  simplex_fbm: {
    title: "Simplex fBm",
    body: "Ken Perlin's improved noise uses simplex lattice (triangular in 2D) reducing directional artifacts and O(n²) complexity from Perlin's O(2^n).",
    formula: "simplex basis uses (√3−1)/2 skewing"
  },
  simplex_turbulence: {
    title: "Simplex Turbulence",
    body: "Absolute value turbulence applied to simplex basis, inheriting its lower complexity and better isotropy compared to classic Perlin.",
    formula: "f(x) = Σ gain^i · |simplex(x · lac^i)|"
  },
  worley_f1: {
    title: "Worley / Cellular F1",
    body: "Steven Worley's cellular noise. At each point, finds the distance to the nearest of N randomly placed feature points. F1 = distance to closest. Creates Voronoi-cell textures.",
    formula: "F1(x) = min_i { dist(x, featurePoint_i) }"
  },
  worley_f2: {
    title: "Worley F2",
    body: "Second-nearest feature point distance in Worley's cellular noise. F2 produces bubbly, organic cell-wall patterns with smoother transitions.",
    formula: "F2(x) = 2nd min_i { dist(x, fp_i) }"
  },
  worley_f2_f1: {
    title: "Worley F2 − F1",
    body: "The difference F2−F1 isolates cell edges with high precision, producing thin, sharp veins and crack-like network structures.",
    formula: "F2(x) − F1(x) ≈ 0 at cell edges"
  },
  worley_crease: {
    title: "Worley Crease",
    body: "|F1+F2| produces creased, crystalline patterns with ridge formations. Useful for rock and crystal material synthesis.",
    formula: "|F1(x) + F2(x)|"
  },
  value_fbm: {
    title: "Value fBm",
    body: "Value noise assigns random values to lattice points and interpolates smoothly (quintic S-curve). fBm stacks octaves for detail. Simpler than gradient noise but prone to square artifacts.",
    formula: "f(x,y) = bilerp(rand[i,j], …) using fade(t) = 6t⁵−15t⁴+10t³"
  },
  white: {
    title: "White Noise",
    body: "Uniform power across all frequencies. Each sample is i.i.d. Gaussian. Autocorrelation is a Dirac delta. Power spectral density is flat: S(f) ∝ f⁰.",
    formula: "S(f) = σ² (constant), PSD = flat"
  },
  pink: {
    title: "Pink Noise (1/f)",
    body: "Power spectral density S(f) ∝ 1/f. Generated via Voss-McCartney algorithm stacking multiple white-noise generators with octave-band resets. Ubiquitous in natural systems.",
    formula: "S(f) ∝ f⁻¹, β = 1"
  },
  brown: {
    title: "Brownian / Red Noise",
    body: "Integration of white noise. S(f) ∝ 1/f². Models Brownian motion, random walk. High energy at low frequencies, very smooth appearance.",
    formula: "S(f) ∝ f⁻², β = 2"
  },
  blue: {
    title: "Blue Noise",
    body: "Power increases with frequency: S(f) ∝ f. Anti-correlated samples minimize low-frequency components. Used in dithering, halftoning, sampling theory.",
    formula: "S(f) ∝ f, β = −1"
  },
  violet: {
    title: "Violet Noise",
    body: "Differentiated white noise. S(f) ∝ f². Maximum energy at high frequencies. Alias: purple noise. The spectral dual of Brownian motion.",
    formula: "S(f) ∝ f², β = −2"
  },
};

// ═════════════════════════════════════════════════════════════════════════════
//  Application State
// ═════════════════════════════════════════════════════════════════════════════
const State = {
  algorithm:   "perlin_fbm",
  width:       512,
  height:      512,
  scale:       4.0,
  octaves:     6,
  lacunarity:  2.0,
  gain:        0.5,
  warpStrength:1.5,
  contrast:    1.0,
  brightness:  0.0,
  seed:        42,
  colormap:    0,
  invert:      false,
  seamless:    false,
  customLow:   [0,0,0],
  customHigh:  [255,255,255],

  // Runtime
  lastResult:  null,   // { image_b64, stats, params }
  rawData:     null,   // Float32Array H×W
  zoom:        1.0,
  panX:        0,
  panY:        0,
  animating:   false,

  // History
  history:     [],
};

// ═════════════════════════════════════════════════════════════════════════════
//  DOM references (cached once)
// ═════════════════════════════════════════════════════════════════════════════
const $ = id => document.getElementById(id);
const DOM = {
  engineBadge:  $("engine-badge"),
  badgeText:    $("engine-badge").querySelector(".badge-text"),
  perfMs:       $("perf-ms"),
  algoGrid:     $("algo-grid"),
  cmapGrid:     $("colormap-grid"),
  customColors: $("custom-colors"),
  cLow:         $("c-low"),
  cHigh:        $("c-high"),
  pScale:       $("p-scale"),    vScale:      $("v-scale"),
  pOctaves:     $("p-octaves"),  vOctaves:    $("v-octaves"),
  pLacunarity:  $("p-lacunarity"),vLacunarity:$("v-lacunarity"),
  pGain:        $("p-gain"),     vGain:       $("v-gain"),
  pWarp:        $("p-warp"),     vWarp:       $("v-warp"),
  pContrast:    $("p-contrast"), vContrast:   $("v-contrast"),
  pBrightness:  $("p-brightness"),vBrightness:$("v-brightness"),
  pSeed:        $("p-seed"),
  btnRandSeed:  $("btn-rand-seed"),
  pInvert:      $("p-invert"),
  pSeamless:    $("p-seamless"),
  btnGenerate:  $("btn-generate"),
  btnExportPng: $("btn-export-png"),
  btnExportRaw: $("btn-export-raw"),
  btnExportJson:$("btn-export-json"),
  animSpeed:    $("anim-speed"),  vAnimSpeed: $("v-animspeed"),
  animFps:      $("anim-fps"),    vAnimFps:   $("v-animfps"),
  btnAnimStart: $("btn-anim-start"),
  btnAnimStop:  $("btn-anim-stop"),
  mainCanvas:   $("main-canvas"),
  canvas3d:     $("canvas-3d"),
  canvasInfo:   $("canvas-info"),
  crosshair:    $("crosshair"),
  zoomIn:       $("btn-zoom-in"),
  zoomOut:      $("btn-zoom-out"),
  zoomFit:      $("btn-zoom-fit"),
  zoomLevel:    $("zoom-level"),
  chartHist:    $("chart-histogram"),
  chartAutocorr:$("chart-autocorr"),
  chartSpectrum:$("chart-spectrum"),
  chartWaveform:$("chart-waveform"),
  waveRow:      $("wave-row"),
  stMean:       $("st-mean"),
  stStd:        $("st-std"),
  stVar:        $("st-var"),
  stMin:        $("st-min"),
  stMax:        $("st-max"),
  stSkew:       $("st-skew"),
  stKurt:       $("st-kurt"),
  theoryCard:   $("theory-card"),
  historyList:  $("history-list"),
  loading:      $("loading-overlay"),
  loadingText:  $("loading-text"),
  tooltip:      $("tooltip"),
};

// ═════════════════════════════════════════════════════════════════════════════
//  Chart.js instances (persistent, updated in-place)
// ═════════════════════════════════════════════════════════════════════════════
const Charts = {};

const CHART_DEFAULTS = {
  backgroundColor: "transparent",
  borderColor: "#a3ff5a",
  color: "#7a8c7a",
  gridColor: "rgba(30,36,36,0.8)",
};

function mkChart(canvas, type, data, opts = {}) {
  Chart.defaults.color = CHART_DEFAULTS.color;
  Chart.defaults.borderColor = CHART_DEFAULTS.gridColor;
  return new Chart(canvas, {
    type,
    data,
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 200 },
      plugins: { legend: { display: false }, tooltip: { titleFont: { family: "Space Mono" }, bodyFont: { family: "Space Mono" } } },
      scales: {
        x: { grid: { color: CHART_DEFAULTS.gridColor }, ticks: { font: { family: "Space Mono", size: 9 }, maxTicksLimit: 10 } },
        y: { grid: { color: CHART_DEFAULTS.gridColor }, ticks: { font: { family: "Space Mono", size: 9 }, maxTicksLimit: 6 } },
      },
      ...opts,
    },
  });
}

function initCharts() {
  Charts.histogram = mkChart(DOM.chartHist, "bar", {
    labels: [],
    datasets: [{ label: "Density", data: [], backgroundColor: "rgba(163,255,90,0.35)", borderColor: "#a3ff5a", borderWidth: 1 }]
  }, { scales: { x: { ticks: { maxTicksLimit: 8 } } } });

  Charts.autocorr = mkChart(DOM.chartAutocorr, "line", {
    labels: [],
    datasets: [{ label: "ACF", data: [], borderColor: "#47c8ff", borderWidth: 1.5, pointRadius: 0, fill: false, tension: 0 }]
  });

  Charts.spectrum = mkChart(DOM.chartSpectrum, "line", {
    labels: [],
    datasets: [{ label: "PSD", data: [], borderColor: "#ff4a4a", borderWidth: 1.5, pointRadius: 0, fill: true, backgroundColor: "rgba(255,74,74,0.08)", tension: 0 }]
  }, { scales: { x: { type: "linear", title: { display: true, text: "Frequency bin", color: "#4a6050", font: { family: "Space Mono", size: 9 } } }, y: { type: "logarithmic" } } });

  Charts.waveform = mkChart(DOM.chartWaveform, "line", {
    labels: [],
    datasets: [{ label: "Row", data: [], borderColor: "#ffb347", borderWidth: 1, pointRadius: 0, fill: false, tension: 0.2 }]
  });
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI Builder
// ═════════════════════════════════════════════════════════════════════════════
function buildAlgoGrid() {
  let lastGroup = null;
  ALGORITHMS.forEach(alg => {
    if (alg.group !== lastGroup) {
      const lbl = document.createElement("div");
      lbl.className = "algo-group-label";
      lbl.textContent = alg.group.toUpperCase();
      DOM.algoGrid.appendChild(lbl);
      lastGroup = alg.group;
    }
    const btn = document.createElement("button");
    btn.className = "algo-btn" + (alg.id === State.algorithm ? " active" : "");
    btn.dataset.id = alg.id;
    btn.textContent = alg.label;
    btn.title = THEORY[alg.id]?.body || "";
    btn.addEventListener("click", () => selectAlgorithm(alg.id));
    DOM.algoGrid.appendChild(btn);
  });
}

function buildColorMapGrid() {
  COLORMAPS.forEach(cm => {
    const btn = document.createElement("button");
    btn.className = "cmap-btn" + (cm.id === State.colormap ? " active" : "");
    btn.dataset.id = cm.id;
    btn.dataset.label = cm.label;
    const grad = `linear-gradient(90deg, ${cm.stops.join(",")})`;
    btn.style.background = grad;
    btn.title = cm.label;
    btn.addEventListener("click", () => selectColormap(cm.id));
    DOM.cmapGrid.appendChild(btn);
  });
}

// ═════════════════════════════════════════════════════════════════════════════
//  Control bindings
// ═════════════════════════════════════════════════════════════════════════════
function bindSlider(inputEl, displayEl, key, decimals = 2) {
  inputEl.addEventListener("input", () => {
    State[key] = parseFloat(inputEl.value);
    displayEl.textContent = State[key].toFixed(decimals);
  });
}

function bindControls() {
  bindSlider(DOM.pScale,       DOM.vScale,       "scale",        1);
  bindSlider(DOM.pOctaves,     DOM.vOctaves,     "octaves",      0);
  bindSlider(DOM.pLacunarity,  DOM.vLacunarity,  "lacunarity",   2);
  bindSlider(DOM.pGain,        DOM.vGain,        "gain",         2);
  bindSlider(DOM.pWarp,        DOM.vWarp,        "warpStrength", 2);
  bindSlider(DOM.pContrast,    DOM.vContrast,    "contrast",     2);
  bindSlider(DOM.pBrightness,  DOM.vBrightness,  "brightness",   2);
  bindSlider(DOM.animSpeed,    DOM.vAnimSpeed,   "animSpeed",    3);
  bindSlider(DOM.animFps,      DOM.vAnimFps,     "animFps",      0);

  DOM.pSeed.addEventListener("change", () => { State.seed = parseInt(DOM.pSeed.value) || 42; });
  DOM.btnRandSeed.addEventListener("click", () => {
    State.seed = Math.floor(Math.random() * 1e8);
    DOM.pSeed.value = State.seed;
  });
  DOM.pInvert.addEventListener("change",   () => { State.invert   = DOM.pInvert.checked; });
  DOM.pSeamless.addEventListener("change", () => { State.seamless = DOM.pSeamless.checked; });

  // Resolution
  document.querySelectorAll(".res-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".res-btn").forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      State.width  = parseInt(btn.dataset.w);
      State.height = parseInt(btn.dataset.h);
    });
  });

  // Custom colors
  DOM.cLow.addEventListener("input",  () => { State.customLow  = hexToRgb(DOM.cLow.value); });
  DOM.cHigh.addEventListener("input", () => { State.customHigh = hexToRgb(DOM.cHigh.value); });

  // Generate button
  DOM.btnGenerate.addEventListener("click", generate);

  // Export
  DOM.btnExportPng.addEventListener("click",  exportPng);
  DOM.btnExportRaw.addEventListener("click",  exportRaw);
  DOM.btnExportJson.addEventListener("click", exportJson);

  // Animation
  DOM.btnAnimStart.addEventListener("click",  startAnimation);
  DOM.btnAnimStop.addEventListener("click",   stopAnimation);

  // Zoom
  DOM.zoomIn.addEventListener("click",  () => setZoom(State.zoom * 1.25));
  DOM.zoomOut.addEventListener("click", () => setZoom(State.zoom / 1.25));
  DOM.zoomFit.addEventListener("click", () => { State.zoom = 1; State.panX = 0; State.panY = 0; applyTransform(); });

  // Viewer tabs
  document.querySelectorAll(".vtab").forEach(tab => {
    tab.addEventListener("click", () => {
      document.querySelectorAll(".vtab").forEach(t => t.classList.remove("active"));
      document.querySelectorAll(".tab-pane").forEach(p => p.classList.remove("active"));
      tab.classList.add("active");
      document.getElementById("tab-" + tab.dataset.tab).classList.add("active");
      if (tab.dataset.tab === "3d" && State.rawData) render3D(State.rawData);
    });
  });

  // Waveform row
  DOM.waveRow.addEventListener("input", () => {
    if (State.rawData) updateWaveformChart(State.rawData, parseInt(DOM.waveRow.value));
  });

  // Canvas mouse interactions
  setupCanvasInteraction();

  // Keyboard shortcut: G = generate
  document.addEventListener("keydown", e => {
    if (e.key === "g" && !e.target.matches("input")) generate();
    if (e.key === "Escape") stopAnimation();
  });
}

function selectAlgorithm(id) {
  State.algorithm = id;
  document.querySelectorAll(".algo-btn").forEach(b => b.classList.toggle("active", b.dataset.id === id));
  updateTheoryCard(id);
}

function selectColormap(id) {
  State.colormap = id;
  document.querySelectorAll(".cmap-btn").forEach(b => b.classList.toggle("active", parseInt(b.dataset.id) === id));
  DOM.customColors.style.display = id === 7 ? "flex" : "none";
}

function updateTheoryCard(id) {
  const th = THEORY[id];
  if (!th) { DOM.theoryCard.textContent = "No theory entry."; return; }
  DOM.theoryCard.innerHTML = `<h4>${th.title}</h4><p>${th.body}</p><code class="formula">${th.formula}</code>`;
}

// ═════════════════════════════════════════════════════════════════════════════
//  API Layer
// ═════════════════════════════════════════════════════════════════════════════
function buildParams() {
  return {
    algorithm:    State.algorithm,
    width:        State.width,
    height:       State.height,
    scale:        State.scale,
    octaves:      State.octaves,
    lacunarity:   State.lacunarity,
    gain:         State.gain,
    warp_strength:State.warpStrength,
    seed:         State.seed,
    colormap:     State.colormap,
    invert:       State.invert,
    contrast:     State.contrast,
    brightness:   State.brightness,
    custom_low:   State.customLow,
    custom_high:  State.customHigh,
    offset_x:     0,
    offset_y:     0,
    time_z:       0,
  };
}

async function fetchGenerate(params) {
  const res = await fetch(`${API_BASE}/api/generate`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(params),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

async function fetchInfo() {
  try {
    const res  = await fetch(`${API_BASE}/api/info`);
    const data = await res.json();
    updateEngineBadge(data.engine);
  } catch (e) {
    setEngineBadge("error", "OFFLINE");
  }
}

function updateEngineBadge(engine) {
  if (engine.native) {
    setEngineBadge("native", `C++ ENGINE v${engine.version}`);
  } else {
    setEngineBadge("python", "PYTHON ENGINE");
  }
}

function setEngineBadge(cls, text) {
  DOM.engineBadge.className = "engine-badge " + cls;
  DOM.badgeText.textContent = text;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Generate
// ═════════════════════════════════════════════════════════════════════════════
async function generate() {
  if (DOM.btnGenerate.disabled) return;
  showLoading("GENERATING…");
  DOM.btnGenerate.disabled = true;

  const params = buildParams();
  try {
    const data = await fetchGenerate(params);
    if (data.error) throw new Error(data.error);

    State.lastResult = data;
    DOM.perfMs.textContent = data.elapsed_ms;

    // Display image
    await displayImage(data.image_b64);

    // Update stats
    updateStats(data.stats);

    // Update charts
    updateAnalysisCharts(data.stats);

    // Push history
    pushHistory(data.image_b64, params.algorithm);

    // 3D if visible
    if (document.getElementById("tab-3d").classList.contains("active") && State.rawData)
      render3D(State.rawData);

  } catch (err) {
    console.error("[generate]", err);
    DOM.canvasInfo.textContent = "ERROR: " + err.message;
  } finally {
    hideLoading();
    DOM.btnGenerate.disabled = false;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Image display
// ═════════════════════════════════════════════════════════════════════════════
async function displayImage(b64) {
  return new Promise(resolve => {
    const img   = new Image();
    img.onload  = () => {
      const canvas = DOM.mainCanvas;
      canvas.width  = img.width;
      canvas.height = img.height;
      const ctx = canvas.getContext("2d");
      ctx.drawImage(img, 0, 0);

      // Extract raw pixel data for waveform / 3D
      const imageData = ctx.getImageData(0, 0, img.width, img.height);
      const raw = new Float32Array(img.width * img.height);
      for (let i = 0; i < raw.length; i++)
        raw[i] = (imageData.data[i * 4] / 255 + imageData.data[i * 4 + 1] / 255 + imageData.data[i * 4 + 2] / 255) / 3;
      State.rawData = raw;
      DOM.waveRow.max = img.height - 1;
      DOM.waveRow.value = Math.floor(img.height / 2);
      updateWaveformChart(raw, Math.floor(img.height / 2));
      resolve();
    };
    img.src = "data:image/png;base64," + b64;
  });
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stats display
// ═════════════════════════════════════════════════════════════════════════════
function updateStats(stats) {
  const fmt = (v, d = 4) => (typeof v === "number" ? v.toFixed(d) : "—");
  DOM.stMean.textContent  = fmt(stats.mean);
  DOM.stStd.textContent   = fmt(stats.std_dev);
  DOM.stVar.textContent   = fmt(stats.variance);
  DOM.stMin.textContent   = fmt(stats.min_val);
  DOM.stMax.textContent   = fmt(stats.max_val);
  DOM.stSkew.textContent  = fmt(stats.skewness, 3);
  DOM.stKurt.textContent  = fmt(stats.kurtosis, 3);
}

function updateAnalysisCharts(stats) {
  // Histogram
  const hist = stats.histogram || [];
  Charts.histogram.data.labels   = hist.map((_, i) => (i / hist.length).toFixed(2));
  Charts.histogram.data.datasets[0].data = hist;
  Charts.histogram.update("none");

  // Autocorrelation
  const acf = stats.autocorrelation || [];
  Charts.autocorr.data.labels   = acf.map((_, i) => i);
  Charts.autocorr.data.datasets[0].data = acf;
  Charts.autocorr.update("none");

  // Power spectrum (log y)
  const psd = stats.power_spectrum || [];
  Charts.spectrum.data.labels   = psd.map((_, i) => i);
  Charts.spectrum.data.datasets[0].data = psd.map(v => Math.max(v, 1e-12));
  Charts.spectrum.update("none");
}

function updateWaveformChart(raw, row) {
  const W = State.width;
  const offset = row * W;
  const rowData = Array.from(raw.slice(offset, offset + W));
  Charts.waveform.data.labels   = rowData.map((_, i) => i);
  Charts.waveform.data.datasets[0].data = rowData;
  Charts.waveform.update("none");
}

// ═════════════════════════════════════════════════════════════════════════════
//  3D Surface Renderer (pure WebGL via raw canvas)
// ═════════════════════════════════════════════════════════════════════════════
const _3D = (() => {
  let gl, prog, vao, vertCount;
  let rotX = -0.4, rotY = 0.3;
  let lastMX, lastMY, dragging = false;
  let uniformLocs = {};

  const VS = `#version 300 es
  in vec3 aPos; in float aHeight;
  uniform mat4 uMVP;
  out float vH;
  void main() { gl_Position = uMVP * vec4(aPos, 1.0); vH = aHeight; }`;

  const FS = `#version 300 es
  precision highp float;
  in float vH;
  out vec4 fragColor;
  vec3 inferno(float t) {
    t = clamp(t,0.,1.);
    float r = clamp(3.54*t-0.34,0.,1.);
    float g = clamp(2.*t*t-1.5*t+.1,0.,1.);
    float b = clamp(sin(3.14159*t)*.8,0.,1.);
    return vec3(r,g,b);
  }
  void main() { fragColor = vec4(inferno(vH), 1.0); }`;

  function compileShader(gl, type, src) {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS))
      console.error(gl.getShaderInfoLog(s));
    return s;
  }

  function mat4Mul(a, b) {
    const r = new Float32Array(16);
    for (let i = 0; i < 4; i++)
      for (let j = 0; j < 4; j++)
        for (let k = 0; k < 4; k++)
          r[i*4+j] += a[i*4+k] * b[k*4+j];
    return r;
  }

  function perspective(fov, aspect, near, far) {
    const f = 1 / Math.tan(fov / 2), r = 1 / (near - far);
    return new Float32Array([f/aspect,0,0,0, 0,f,0,0, 0,0,(near+far)*r,-1, 0,0,near*far*r*2,0]);
  }

  function rotateX(a) {
    const c = Math.cos(a), s = Math.sin(a);
    return new Float32Array([1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1]);
  }
  function rotateY(a) {
    const c = Math.cos(a), s = Math.sin(a);
    return new Float32Array([c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1]);
  }
  function translate(tx, ty, tz) {
    return new Float32Array([1,0,0,0, 0,1,0,0, 0,0,1,0, tx,ty,tz,1]);
  }

  function init(canvas) {
    gl = canvas.getContext("webgl2", { antialias: true, alpha: false });
    if (!gl) { console.warn("WebGL2 not available"); return false; }

    const vs = compileShader(gl, gl.VERTEX_SHADER,   VS);
    const fs = compileShader(gl, gl.FRAGMENT_SHADER, FS);
    prog = gl.createProgram();
    gl.attachShader(prog, vs); gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    uniformLocs.MVP = gl.getUniformLocation(prog, "uMVP");

    gl.clearColor(0.03, 0.04, 0.04, 1);
    gl.enable(gl.DEPTH_TEST);

    // Mouse drag
    canvas.addEventListener("mousedown", e => { dragging=true; lastMX=e.clientX; lastMY=e.clientY; });
    window.addEventListener("mouseup",   () => { dragging=false; });
    window.addEventListener("mousemove", e => {
      if (!dragging) return;
      rotY += (e.clientX - lastMX) * 0.005;
      rotX += (e.clientY - lastMY) * 0.005;
      rotX = Math.max(-1.2, Math.min(0, rotX));
      lastMX=e.clientX; lastMY=e.clientY;
      if (gl && prog && vao) drawFrame();
    });
    return true;
  }

  function buildMesh(raw, W, H) {
    const DX = 32, DY = 32;   // downsample for performance
    const sw = Math.floor(W / DX), sh = Math.floor(H / DY);
    const positions = [], heights = [];
    for (let y = 0; y <= sh; y++) {
      for (let x = 0; x <= sw; x++) {
        const u  = x / sw, v = y / sh;
        const xi = Math.min(Math.floor(u * W), W-1);
        const yi = Math.min(Math.floor(v * H), H-1);
        const h  = raw[yi * W + xi] * 0.4;
        positions.push(u * 2 - 1, h, v * 2 - 1);
        heights.push(raw[yi * W + xi]);
      }
    }
    const indices = [];
    for (let y = 0; y < sh; y++)
      for (let x = 0; x < sw; x++) {
        const tl = y*(sw+1)+x, tr=tl+1, bl=tl+(sw+1), br=bl+1;
        indices.push(tl,bl,tr, tr,bl,br);
      }
    vertCount = indices.length;

    vao = gl.createVertexArray();
    gl.bindVertexArray(vao);

    const posVBO = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, posVBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);
    const posLoc = gl.getAttribLocation(prog, "aPos");
    gl.enableVertexAttribArray(posLoc);
    gl.vertexAttribPointer(posLoc, 3, gl.FLOAT, false, 0, 0);

    const hVBO = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, hVBO);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(heights), gl.STATIC_DRAW);
    const hLoc = gl.getAttribLocation(prog, "aHeight");
    gl.enableVertexAttribArray(hLoc);
    gl.vertexAttribPointer(hLoc, 1, gl.FLOAT, false, 0, 0);

    const ibo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(indices), gl.STATIC_DRAW);

    gl.bindVertexArray(null);
  }

  function drawFrame() {
    const canvas = DOM.canvas3d;
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.useProgram(prog);

    const proj = perspective(0.85, canvas.width/canvas.height, 0.01, 100);
    const view = translate(0, -0.3, -2.2);
    const rx   = rotateX(rotX);
    const ry   = rotateY(rotY);
    const mvp  = mat4Mul(proj, mat4Mul(view, mat4Mul(ry, rx)));
    gl.uniformMatrix4fv(uniformLocs.MVP, false, mvp);

    gl.bindVertexArray(vao);
    gl.drawElements(gl.TRIANGLES, vertCount, gl.UNSIGNED_INT, 0);
    gl.bindVertexArray(null);
  }

  return {
    render(raw) {
      const canvas = DOM.canvas3d;
      canvas.width  = canvas.clientWidth  * devicePixelRatio;
      canvas.height = canvas.clientHeight * devicePixelRatio;
      if (!gl && !init(canvas)) return;
      buildMesh(raw, State.width, State.height);
      drawFrame();
    }
  };
})();

function render3D(raw) { _3D.render(raw); }

// ═════════════════════════════════════════════════════════════════════════════
//  Canvas zoom / pan / crosshair
// ═════════════════════════════════════════════════════════════════════════════
function setupCanvasInteraction() {
  const wrapper = DOM.mainCanvas.parentElement;

  // Zoom via wheel
  wrapper.addEventListener("wheel", e => {
    e.preventDefault();
    setZoom(State.zoom * (e.deltaY < 0 ? 1.1 : 0.9));
  }, { passive: false });

  // Crosshair
  DOM.mainCanvas.addEventListener("mousemove", e => {
    const rect = DOM.mainCanvas.getBoundingClientRect();
    const x = Math.floor((e.clientX - rect.left) / State.zoom);
    const y = Math.floor((e.clientY - rect.top)  / State.zoom);
    DOM.crosshair.style.left = e.clientX - rect.left + "px";
    DOM.crosshair.style.top  = e.clientY - rect.top  + "px";
    DOM.crosshair.style.opacity = "1";

    if (State.rawData && x >= 0 && x < State.width && y >= 0 && y < State.height) {
      const v = State.rawData[y * State.width + x];
      DOM.canvasInfo.textContent = `x:${x}  y:${y}  v:${v.toFixed(4)}`;
    }
  });
  DOM.mainCanvas.addEventListener("mouseleave", () => { DOM.crosshair.style.opacity = "0"; });

  // Pan (drag)
  let panning = false, pStartX, pStartY, pBase = [0, 0];
  wrapper.addEventListener("mousedown", e => { if (e.button === 1 || e.altKey) { panning=true; pStartX=e.clientX; pStartY=e.clientY; pBase=[State.panX,State.panY]; } });
  window.addEventListener("mousemove",  e => { if (!panning) return; State.panX=pBase[0]+(e.clientX-pStartX); State.panY=pBase[1]+(e.clientY-pStartY); applyTransform(); });
  window.addEventListener("mouseup",    () => { panning=false; });
}

function setZoom(z) {
  State.zoom = Math.max(0.1, Math.min(8, z));
  DOM.zoomLevel.textContent = Math.round(State.zoom * 100) + "%";
  applyTransform();
}

function applyTransform() {
  DOM.mainCanvas.style.transform = `translate(${State.panX}px, ${State.panY}px) scale(${State.zoom})`;
  DOM.mainCanvas.style.transformOrigin = "center center";
}

// ═════════════════════════════════════════════════════════════════════════════
//  WebSocket Animation
// ═════════════════════════════════════════════════════════════════════════════
let socket = null;

function ensureSocket() {
  if (!socket) {
    socket = io(WS_URL, { transports: ["websocket", "polling"] });
    socket.on("frame", async data => {
      await displayImage(data.image_b64);
      if (data.stats) updateStats(data.stats);
    });
    socket.on("error", d => { console.error("[ws]", d.message); stopAnimation(); });
  }
  return socket;
}

function startAnimation() {
  if (State.animating) return;
  State.animating = true;
  DOM.btnAnimStart.style.opacity = "0.5";

  const s = ensureSocket();
  const params = buildParams();
  params.time_z = 0;
  s.emit("start_animation", {
    params,
    speed: parseFloat(DOM.animSpeed.value),
    fps:   parseInt(DOM.animFps.value),
  });
}

function stopAnimation() {
  if (!State.animating) return;
  State.animating = false;
  DOM.btnAnimStart.style.opacity = "1";
  if (socket) socket.emit("stop_animation");
}

// ═════════════════════════════════════════════════════════════════════════════
//  History
// ═════════════════════════════════════════════════════════════════════════════
function pushHistory(b64, alg) {
  const item = { b64, alg, time: Date.now() };
  State.history.unshift(item);
  if (State.history.length > 12) State.history.pop();
  renderHistory();
}

function renderHistory() {
  DOM.historyList.innerHTML = "";
  State.history.forEach(item => {
    const div  = document.createElement("div");
    div.className = "history-item";
    const img  = document.createElement("img");
    img.className = "history-thumb";
    img.src = "data:image/png;base64," + item.b64;
    const meta = document.createElement("div");
    meta.className = "history-meta";
    const algLabel = ALGORITHMS.find(a => a.id === item.alg)?.label || item.alg;
    meta.innerHTML = `<div class="history-alg">${algLabel}</div><div class="history-time">${new Date(item.time).toLocaleTimeString()}</div>`;
    div.appendChild(img); div.appendChild(meta);
    div.addEventListener("click", () => { displayImage(item.b64); });
    DOM.historyList.appendChild(div);
  });
}

// ═════════════════════════════════════════════════════════════════════════════
//  Export
// ═════════════════════════════════════════════════════════════════════════════
function exportPng() {
  if (!State.lastResult) return;
  const a = document.createElement("a");
  a.href = "data:image/png;base64," + State.lastResult.image_b64;
  a.download = `noise_${State.algorithm}_${State.seed}.png`;
  a.click();
}

function exportRaw() {
  if (!State.rawData) return;
  const arr = State.rawData;
  const rows = [];
  for (let y = 0; y < State.height; y++) {
    const row = [];
    for (let x = 0; x < State.width; x++) row.push(arr[y * State.width + x].toFixed(6));
    rows.push(row.join(","));
  }
  const blob = new Blob([rows.join("\n")], { type: "text/csv" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `noise_raw_${State.algorithm}_${State.seed}.csv`;
  a.click();
}

function exportJson() {
  if (!State.lastResult) return;
  const out = {
    params: State.lastResult.params,
    stats:  State.lastResult.stats,
    engine: State.lastResult.engine,
    generated_at: new Date().toISOString(),
  };
  const blob = new Blob([JSON.stringify(out, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `noise_meta_${State.algorithm}_${State.seed}.json`;
  a.click();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Utility
// ═════════════════════════════════════════════════════════════════════════════
function hexToRgb(hex) {
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  return [r, g, b];
}

function showLoading(text) {
  DOM.loadingText.textContent = text;
  DOM.loading.classList.remove("hidden");
}
function hideLoading() { DOM.loading.classList.add("hidden"); }

// ═════════════════════════════════════════════════════════════════════════════
//  Boot
// ═════════════════════════════════════════════════════════════════════════════
async function boot() {
  buildAlgoGrid();
  buildColorMapGrid();
  initCharts();
  bindControls();
  selectAlgorithm(State.algorithm);
  updateTheoryCard(State.algorithm);

  // Check backend
  await fetchInfo();

  // Auto-generate on load
  await generate();
}

document.addEventListener("DOMContentLoaded", boot);
