#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "noise_engine.hpp"

namespace py = pybind11;
using namespace pybind11::literals;
using namespace noise;

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: convert vector<uint8_t> to numpy array (H, W, 4) uint8
// ─────────────────────────────────────────────────────────────────────────────
py::array_t<uint8_t> rgba_to_numpy(const std::vector<uint8_t>& data, int h, int w) {
    return py::array_t<uint8_t>(
        {h, w, 4},
        {w * 4, 4, 1},
        data.data()
    );
}

py::array_t<double> raw_to_numpy(const std::vector<double>& data, int h, int w) {
    return py::array_t<double>(
        {h, w},
        {w * sizeof(double), sizeof(double)},
        data.data()
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pythonic wrapper for GeneratorParams
// ─────────────────────────────────────────────────────────────────────────────
GeneratorParams make_params(py::dict d) {
    GeneratorParams p;

    auto get = [&](const char* k, auto def) {
        if (d.contains(k)) return py::cast<decltype(def)>(d[k]);
        return def;
    };

    static const std::unordered_map<std::string, Algorithm> alg_map = {
        {"perlin_fbm",         Algorithm::PERLIN_FBM},
        {"perlin_turbulence",  Algorithm::PERLIN_TURBULENCE},
        {"perlin_ridged",      Algorithm::PERLIN_RIDGED},
        {"perlin_domain_warp", Algorithm::PERLIN_DOMAIN_WARP},
        {"simplex_fbm",        Algorithm::SIMPLEX_FBM},
        {"simplex_turbulence", Algorithm::SIMPLEX_TURBULENCE},
        {"worley_f1",          Algorithm::WORLEY_F1},
        {"worley_f2",          Algorithm::WORLEY_F2},
        {"worley_f2_f1",       Algorithm::WORLEY_F2_F1},
        {"worley_crease",      Algorithm::WORLEY_CREASE},
        {"value_fbm",          Algorithm::VALUE_FBM},
        {"white",              Algorithm::WHITE},
        {"pink",               Algorithm::PINK},
        {"brown",              Algorithm::BROWN},
        {"blue",               Algorithm::BLUE},
        {"violet",             Algorithm::VIOLET},
    };

    std::string alg_str = get("algorithm", std::string("perlin_fbm"));
    auto it = alg_map.find(alg_str);
    p.algorithm   = (it != alg_map.end()) ? it->second : Algorithm::PERLIN_FBM;
    p.width       = get("width",        512);
    p.height      = get("height",       512);
    p.scale       = get("scale",        4.0);
    p.octaves     = get("octaves",      6);
    p.lacunarity  = get("lacunarity",   2.0);
    p.gain        = get("gain",         0.5);
    p.warp_strength = get("warp_strength", 1.5);
    p.seed        = static_cast<uint64_t>(get("seed", 42));
    p.offset_x    = get("offset_x",    0.0);
    p.offset_y    = get("offset_y",    0.0);
    p.time_z      = get("time_z",      0.0);
    p.colormap    = get("colormap",    0);
    p.invert      = get("invert",      false);
    p.normalize   = get("normalize",   true);
    p.contrast    = get("contrast",    1.0);
    p.brightness  = get("brightness",  0.0);

    if (d.contains("custom_low")) {
        auto v = py::cast<std::vector<int>>(d["custom_low"]);
        p.custom_low = {static_cast<uint8_t>(v[0]), static_cast<uint8_t>(v[1]), static_cast<uint8_t>(v[2])};
    }
    if (d.contains("custom_high")) {
        auto v = py::cast<std::vector<int>>(d["custom_high"]);
        p.custom_high = {static_cast<uint8_t>(v[0]), static_cast<uint8_t>(v[1]), static_cast<uint8_t>(v[2])};
    }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module definition
// ─────────────────────────────────────────────────────────────────────────────
PYBIND11_MODULE(noise_engine, m) {
    m.doc() = "Enterprise-grade noise generation engine (C++ via pybind11)";

    // ── PerlinNoise ──────────────────────────────────────────────────────────
    py::class_<PerlinNoise>(m, "PerlinNoise")
        .def(py::init<uint64_t>(), py::arg("seed") = 0)
        .def("reseed", &PerlinNoise::reseed)
        .def("noise1d", &PerlinNoise::noise1d)
        .def("noise2d", &PerlinNoise::noise2d)
        .def("noise3d", &PerlinNoise::noise3d)
        .def("fbm", &PerlinNoise::fbm,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5)
        .def("turbulence", &PerlinNoise::turbulence,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5)
        .def("ridged", &PerlinNoise::ridged,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5, py::arg("offset") = 1.0)
        .def("domain_warp", &PerlinNoise::domainWarp,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5, py::arg("warp_strength") = 1.5);

    // ── SimplexNoise ─────────────────────────────────────────────────────────
    py::class_<SimplexNoise>(m, "SimplexNoise")
        .def(py::init<uint64_t>(), py::arg("seed") = 0)
        .def("reseed", &SimplexNoise::reseed)
        .def("noise2d", &SimplexNoise::noise2d)
        .def("noise3d", &SimplexNoise::noise3d)
        .def("fbm", &SimplexNoise::fbm,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5)
        .def("turbulence", &SimplexNoise::turbulence,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5);

    // ── WorleyNoise ───────────────────────────────────────────────────────────
    py::enum_<WorleyNoise::Mode>(m, "WorleyMode")
        .value("F1",           WorleyNoise::Mode::F1)
        .value("F2",           WorleyNoise::Mode::F2)
        .value("F2_F1",        WorleyNoise::Mode::F2_F1)
        .value("F1_F2_CREASE", WorleyNoise::Mode::F1_F2_CREASE);

    py::class_<WorleyNoise>(m, "WorleyNoise")
        .def(py::init<uint64_t>(), py::arg("seed") = 0)
        .def("noise", &WorleyNoise::noise,
             py::arg("x"), py::arg("y"), py::arg("mode") = WorleyNoise::Mode::F1)
        .def("fbm", &WorleyNoise::fbm,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 4,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5,
             py::arg("mode") = WorleyNoise::Mode::F1);

    // ── ValueNoise ────────────────────────────────────────────────────────────
    py::class_<ValueNoise>(m, "ValueNoise")
        .def(py::init<uint64_t>(), py::arg("seed") = 0)
        .def("noise2d", &ValueNoise::noise2d)
        .def("fbm", &ValueNoise::fbm,
             py::arg("x"), py::arg("y"), py::arg("octaves") = 6,
             py::arg("lacunarity") = 2.0, py::arg("gain") = 0.5);

    // ── SpectralNoise ─────────────────────────────────────────────────────────
    py::class_<SpectralNoise>(m, "SpectralNoise")
        .def(py::init<uint64_t>(), py::arg("seed") = 42)
        .def("white",          &SpectralNoise::white)
        .def("pink",           &SpectralNoise::pink)
        .def("brown",          &SpectralNoise::brown)
        .def("blue",           &SpectralNoise::blue)
        .def("violet",         &SpectralNoise::violet)
        .def("power_spectrum", &SpectralNoise::powerSpectrum,
             py::arg("n"), py::arg("exponent") = 1.0);

    // ── NoiseStats ────────────────────────────────────────────────────────────
    py::class_<NoiseStats>(m, "NoiseStats")
        .def_readonly("mean",              &NoiseStats::mean)
        .def_readonly("variance",          &NoiseStats::variance)
        .def_readonly("std_dev",           &NoiseStats::std_dev)
        .def_readonly("min_val",           &NoiseStats::min_val)
        .def_readonly("max_val",           &NoiseStats::max_val)
        .def_readonly("skewness",          &NoiseStats::skewness)
        .def_readonly("kurtosis",          &NoiseStats::kurtosis)
        .def_readonly("histogram",         &NoiseStats::histogram)
        .def_readonly("autocorrelation",   &NoiseStats::autocorrelation)
        .def_readonly("power_spectrum",    &NoiseStats::power_spectrum)
        .def("to_dict", [](const NoiseStats& s) {
            return py::dict(
                "mean"_a = s.mean, "variance"_a = s.variance,
                "std_dev"_a = s.std_dev, "min_val"_a = s.min_val,
                "max_val"_a = s.max_val, "skewness"_a = s.skewness,
                "kurtosis"_a = s.kurtosis, "histogram"_a = s.histogram,
                "autocorrelation"_a = s.autocorrelation,
                "power_spectrum"_a = s.power_spectrum
            );
        });

    // ── NoiseGenerator.Result ─────────────────────────────────────────────────
    py::class_<NoiseGenerator::Result>(m, "GeneratorResult")
        .def_readonly("width",  &NoiseGenerator::Result::width)
        .def_readonly("height", &NoiseGenerator::Result::height)
        .def_readonly("stats",  &NoiseGenerator::Result::stats)
        .def("rgba_array", [](const NoiseGenerator::Result& r) {
            return rgba_to_numpy(r.rgba, r.height, r.width);
        })
        .def("raw_array", [](const NoiseGenerator::Result& r) {
            return raw_to_numpy(r.raw, r.height, r.width);
        });

    // ── NoiseGenerator ────────────────────────────────────────────────────────
    py::class_<NoiseGenerator>(m, "NoiseGenerator")
        .def(py::init<>())
        .def("generate", [](NoiseGenerator& g, py::dict params) {
            return g.generate(make_params(params));
        })
        .def("generate_animation_frame", [](NoiseGenerator& g, py::dict params, double t) {
            auto p = make_params(params);
            p.time_z = t;
            return g.generate(p);
        });

    // ── Standalone analyze ────────────────────────────────────────────────────
    m.def("analyze_noise", [](const std::vector<double>& data, int bins, int lags) {
        return analyzeNoise(data, bins, lags);
    }, py::arg("data"), py::arg("bins") = 64, py::arg("lags") = 64);

    // ── Enum Algorithm (for documentation) ───────────────────────────────────
    py::enum_<Algorithm>(m, "Algorithm")
        .value("PERLIN_FBM",         Algorithm::PERLIN_FBM)
        .value("PERLIN_TURBULENCE",  Algorithm::PERLIN_TURBULENCE)
        .value("PERLIN_RIDGED",      Algorithm::PERLIN_RIDGED)
        .value("PERLIN_DOMAIN_WARP", Algorithm::PERLIN_DOMAIN_WARP)
        .value("SIMPLEX_FBM",        Algorithm::SIMPLEX_FBM)
        .value("SIMPLEX_TURBULENCE", Algorithm::SIMPLEX_TURBULENCE)
        .value("WORLEY_F1",          Algorithm::WORLEY_F1)
        .value("WORLEY_F2",          Algorithm::WORLEY_F2)
        .value("WORLEY_F2_F1",       Algorithm::WORLEY_F2_F1)
        .value("WORLEY_CREASE",      Algorithm::WORLEY_CREASE)
        .value("VALUE_FBM",          Algorithm::VALUE_FBM)
        .value("WHITE",              Algorithm::WHITE)
        .value("PINK",               Algorithm::PINK)
        .value("BROWN",              Algorithm::BROWN)
        .value("BLUE",               Algorithm::BLUE)
        .value("VIOLET",             Algorithm::VIOLET);

    // ── Version ───────────────────────────────────────────────────────────────
    m.attr("__version__") = "2.0.0";
    m.attr("__author__")  = "Noise Visualizer Enterprise";
}
