#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <complex>
#include <string>

namespace noise {

// ─────────────────────────────────────────────────────────────────────────────
//  Math Utilities
// ─────────────────────────────────────────────────────────────────────────────
namespace math {
    constexpr double PI  = 3.14159265358979323846;
    constexpr double TAU = 6.28318530717958647692;

    inline double fade(double t) noexcept { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
    inline double lerp(double a, double b, double t) noexcept { return a + t * (b - a); }
    inline double smoothstep(double e0, double e1, double x) noexcept {
        double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }
    inline double smootherstep(double e0, double e1, double x) noexcept {
        double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }
    inline double grad1(int h, double x) noexcept { return (h & 1) ? -x : x; }
    inline double grad2(int h, double x, double y) noexcept {
        int hh = h & 3;
        double u = hh < 2 ? x : y;
        double v = hh < 2 ? y : x;
        return ((hh & 1) ? -u : u) + ((hh & 2) ? -v : v);
    }
    inline double grad3(int h, double x, double y, double z) noexcept {
        int hh = h & 15;
        double u = hh < 8 ? x : y;
        double v = hh < 4 ? y : (hh == 12 || hh == 14 ? x : z);
        return ((hh & 1) ? -u : u) + ((hh & 2) ? -v : v);
    }
    inline double grad4(int h, double x, double y, double z, double w) noexcept {
        int hh = h & 31;
        double a = hh < 24 ? x : y;
        double b = hh < 16 ? y : z;
        double c = hh <  8 ? z : w;
        return ((hh & 1) ? -a : a) + ((hh & 2) ? -b : b) + ((hh & 4) ? -c : c);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Permutation Table
// ─────────────────────────────────────────────────────────────────────────────
class PermTable {
public:
    std::array<int, 512> p{};

    explicit PermTable(uint64_t seed = 0) { reseed(seed); }

    void reseed(uint64_t seed) {
        std::iota(p.begin(), p.begin() + 256, 0);
        std::mt19937_64 rng(seed);
        std::shuffle(p.begin(), p.begin() + 256, rng);
        std::copy(p.begin(), p.begin() + 256, p.begin() + 256);
    }

    int operator[](int i) const noexcept { return p[i & 511]; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Perlin Noise (Classic + Improved)
// ─────────────────────────────────────────────────────────────────────────────
class PerlinNoise {
    PermTable perm;
public:
    explicit PerlinNoise(uint64_t seed = 0) : perm(seed) {}
    void reseed(uint64_t seed) { perm.reseed(seed); }

    double noise1d(double x) const noexcept {
        int X = static_cast<int>(std::floor(x)) & 255;
        x -= std::floor(x);
        double u = math::fade(x);
        double a = math::grad1(perm[X],     x);
        double b = math::grad1(perm[X + 1], x - 1.0);
        return math::lerp(a, b, u);
    }

    double noise2d(double x, double y) const noexcept {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        x -= std::floor(x); y -= std::floor(y);
        double u = math::fade(x), v = math::fade(y);
        int A  = perm[X] + Y,   AA = perm[A],   AB = perm[A + 1];
        int B  = perm[X + 1] + Y, BA = perm[B], BB = perm[B + 1];
        return math::lerp(
            math::lerp(math::grad2(perm[AA], x, y),     math::grad2(perm[BA], x - 1, y),     u),
            math::lerp(math::grad2(perm[AB], x, y - 1), math::grad2(perm[BB], x - 1, y - 1), u), v);
    }

    double noise3d(double x, double y, double z) const noexcept {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        int Z = static_cast<int>(std::floor(z)) & 255;
        x -= std::floor(x); y -= std::floor(y); z -= std::floor(z);
        double u = math::fade(x), v = math::fade(y), w = math::fade(z);
        int A  = perm[X] + Y,     AA = perm[A] + Z,     AB = perm[A + 1] + Z;
        int B  = perm[X + 1] + Y, BA = perm[B] + Z,     BB = perm[B + 1] + Z;
        return math::lerp(
            math::lerp(
                math::lerp(math::grad3(perm[AA],     x, y,     z),     math::grad3(perm[BA],     x - 1, y,     z),     u),
                math::lerp(math::grad3(perm[AB],     x, y - 1, z),     math::grad3(perm[BB],     x - 1, y - 1, z),     u), v),
            math::lerp(
                math::lerp(math::grad3(perm[AA + 1], x, y,     z - 1), math::grad3(perm[BA + 1], x - 1, y,     z - 1), u),
                math::lerp(math::grad3(perm[AB + 1], x, y - 1, z - 1), math::grad3(perm[BB + 1], x - 1, y - 1, z - 1), u), v), w);
    }

    // fBm (fractal Brownian motion) – returns value in [-1,1]
    double fbm(double x, double y, int octaves, double lacunarity, double gain) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * noise2d(x * frequency, y * frequency);
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }

    // Turbulence
    double turbulence(double x, double y, int octaves, double lacunarity, double gain) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * std::abs(noise2d(x * frequency, y * frequency));
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }

    // Ridged multi-fractal
    double ridged(double x, double y, int octaves, double lacunarity, double gain, double offset = 1.0) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0, weight = 1.0;
        for (int i = 0; i < octaves; ++i) {
            double signal = offset - std::abs(noise2d(x * frequency, y * frequency));
            signal *= signal * weight;
            weight  = std::clamp(signal * gain, 0.0, 1.0);
            value  += amplitude * signal;
            frequency *= lacunarity;
            amplitude *= gain * 0.5;
        }
        return value;
    }

    // Domain-warped fbm
    double domainWarp(double x, double y, int octaves, double lacunarity, double gain, double warpStrength) const noexcept {
        double qx = fbm(x,          y,          octaves, lacunarity, gain);
        double qy = fbm(x + 5.2,   y + 1.3,   octaves, lacunarity, gain);
        return fbm(x + warpStrength * qx, y + warpStrength * qy, octaves, lacunarity, gain);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Simplex Noise (2D/3D)
// ─────────────────────────────────────────────────────────────────────────────
class SimplexNoise {
    PermTable perm;
    static constexpr double F2 = 0.5 * (1.7320508075688772 - 1.0); // (sqrt(3)-1)/2
    static constexpr double G2 = (3.0 - 1.7320508075688772) / 6.0;
    static constexpr double F3 = 1.0 / 3.0;
    static constexpr double G3 = 1.0 / 6.0;
    static constexpr int grad3[12][3] = {
        {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
        {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}
    };
    double dot2(const int* g, double x, double y) const noexcept { return g[0]*x + g[1]*y; }
    double dot3(const int* g, double x, double y, double z) const noexcept { return g[0]*x + g[1]*y + g[2]*z; }
public:
    explicit SimplexNoise(uint64_t seed = 0) : perm(seed) {}
    void reseed(uint64_t seed) { perm.reseed(seed); }

    double noise2d(double xin, double yin) const noexcept {
        double s   = (xin + yin) * F2;
        int i  = static_cast<int>(std::floor(xin + s));
        int j  = static_cast<int>(std::floor(yin + s));
        double t   = (i + j) * G2;
        double X0  = i - t, Y0 = j - t;
        double x0  = xin - X0, y0 = yin - Y0;
        int i1 = x0 > y0 ? 1 : 0, j1 = x0 > y0 ? 0 : 1;
        double x1  = x0 - i1 + G2, y1 = y0 - j1 + G2;
        double x2  = x0 - 1.0 + 2.0 * G2, y2 = y0 - 1.0 + 2.0 * G2;
        int ii = i & 255, jj = j & 255;
        int gi0 = perm[ii    + perm[jj   ]] % 12;
        int gi1 = perm[ii+i1 + perm[jj+j1]] % 12;
        int gi2 = perm[ii+1  + perm[jj+1 ]] % 12;
        double t0 = 0.5 - x0*x0 - y0*y0, n0 = t0 < 0 ? 0 : (t0*t0)*(t0*t0)*dot2(grad3[gi0], x0, y0);
        double t1 = 0.5 - x1*x1 - y1*y1, n1 = t1 < 0 ? 0 : (t1*t1)*(t1*t1)*dot2(grad3[gi1], x1, y1);
        double t2 = 0.5 - x2*x2 - y2*y2, n2 = t2 < 0 ? 0 : (t2*t2)*(t2*t2)*dot2(grad3[gi2], x2, y2);
        return 70.0 * (n0 + n1 + n2);
    }

    double noise3d(double xin, double yin, double zin) const noexcept {
        double s = (xin + yin + zin) * F3;
        int i = static_cast<int>(std::floor(xin + s));
        int j = static_cast<int>(std::floor(yin + s));
        int k = static_cast<int>(std::floor(zin + s));
        double t = (i + j + k) * G3;
        double X0 = i - t, Y0 = j - t, Z0 = k - t;
        double x0 = xin - X0, y0 = yin - Y0, z0 = zin - Z0;
        int i1, j1, k1, i2, j2, k2;
        if (x0 >= y0) { if (y0 >= z0) { i1=1;j1=0;k1=0;i2=1;j2=1;k2=0; }
                        else if (x0 >= z0) { i1=1;j1=0;k1=0;i2=1;j2=0;k2=1; }
                        else { i1=0;j1=0;k1=1;i2=1;j2=0;k2=1; } }
        else          { if (y0 < z0) { i1=0;j1=0;k1=1;i2=0;j2=1;k2=1; }
                        else if (x0 < z0) { i1=0;j1=1;k1=0;i2=0;j2=1;k2=1; }
                        else { i1=0;j1=1;k1=0;i2=1;j2=1;k2=0; } }
        double x1=x0-i1+G3, y1=y0-j1+G3, z1=z0-k1+G3;
        double x2=x0-i2+2*G3, y2=y0-j2+2*G3, z2=z0-k2+2*G3;
        double x3=x0-1+3*G3, y3=y0-1+3*G3, z3=z0-1+3*G3;
        int ii=i&255, jj=j&255, kk=k&255;
        int gi0=perm[ii+perm[jj+perm[kk]]]%12;
        int gi1=perm[ii+i1+perm[jj+j1+perm[kk+k1]]]%12;
        int gi2=perm[ii+i2+perm[jj+j2+perm[kk+k2]]]%12;
        int gi3=perm[ii+1+perm[jj+1+perm[kk+1]]]%12;
        double t0=0.6-x0*x0-y0*y0-z0*z0, n0=t0<0?0:(t0*t0)*(t0*t0)*dot3(grad3[gi0],x0,y0,z0);
        double t1=0.6-x1*x1-y1*y1-z1*z1, n1=t1<0?0:(t1*t1)*(t1*t1)*dot3(grad3[gi1],x1,y1,z1);
        double t2=0.6-x2*x2-y2*y2-z2*z2, n2=t2<0?0:(t2*t2)*(t2*t2)*dot3(grad3[gi2],x2,y2,z2);
        double t3=0.6-x3*x3-y3*y3-z3*z3, n3=t3<0?0:(t3*t3)*(t3*t3)*dot3(grad3[gi3],x3,y3,z3);
        return 32.0 * (n0 + n1 + n2 + n3);
    }

    double fbm(double x, double y, int octaves, double lacunarity, double gain) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * noise2d(x * frequency, y * frequency);
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }

    double turbulence(double x, double y, int octaves, double lacunarity, double gain) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * std::abs(noise2d(x * frequency, y * frequency));
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Worley (Cellular) Noise
// ─────────────────────────────────────────────────────────────────────────────
class WorleyNoise {
    uint64_t seed_;
    static uint64_t hash(uint64_t x, uint64_t y, uint64_t s) noexcept {
        uint64_t h = s ^ (x * 2654435761ULL) ^ (y * 2246822519ULL);
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
        return h;
    }
public:
    enum class Mode { F1, F2, F2_F1, F1_F2_CREASE };
    explicit WorleyNoise(uint64_t seed = 0) : seed_(seed) {}

    std::pair<double,double> query(double x, double y) const noexcept {
        int xi = static_cast<int>(std::floor(x));
        int yi = static_cast<int>(std::floor(y));
        double f1 = 1e9, f2 = 1e9;
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int cx = xi + dx, cy = yi + dy;
                uint64_t h  = hash(static_cast<uint64_t>(cx), static_cast<uint64_t>(cy), seed_);
                double   px = cx + static_cast<double>(h         & 0xFFFF) / 65535.0;
                double   py = cy + static_cast<double>((h >> 16) & 0xFFFF) / 65535.0;
                double   d  = std::hypot(x - px, y - py);
                if (d < f1)       { f2 = f1; f1 = d; }
                else if (d < f2)  { f2 = d; }
            }
        }
        return {f1, f2};
    }

    double noise(double x, double y, Mode mode = Mode::F1) const noexcept {
        auto [f1, f2] = query(x, y);
        switch (mode) {
            case Mode::F1:             return f1;
            case Mode::F2:             return f2;
            case Mode::F2_F1:          return f2 - f1;
            case Mode::F1_F2_CREASE:   return std::abs(f1 + f2);
            default:                   return f1;
        }
    }

    double fbm(double x, double y, int octaves, double lacunarity, double gain, Mode mode = Mode::F1) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * noise(x * frequency, y * frequency, mode);
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Value Noise
// ─────────────────────────────────────────────────────────────────────────────
class ValueNoise {
    PermTable perm;
    std::array<double, 256> values{};
public:
    explicit ValueNoise(uint64_t seed = 0) : perm(seed) {
        std::mt19937_64 rng(seed ^ 0xDEADBEEF);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        for (auto& v : values) v = dist(rng);
    }

    double noise2d(double x, double y) const noexcept {
        int X  = static_cast<int>(std::floor(x)) & 255;
        int Y  = static_cast<int>(std::floor(y)) & 255;
        double fx = x - std::floor(x), fy = y - std::floor(y);
        double u  = math::fade(fx), v  = math::fade(fy);
        double v00 = values[perm[X]     + Y   & 255];
        double v10 = values[perm[X + 1] + Y   & 255];
        double v01 = values[perm[X]     + Y+1 & 255];
        double v11 = values[perm[X + 1] + Y+1 & 255];
        return math::lerp(math::lerp(v00, v10, u), math::lerp(v01, v11, u), v);
    }

    double fbm(double x, double y, int octaves, double lacunarity, double gain) const noexcept {
        double value = 0.0, amplitude = 0.5, frequency = 1.0;
        for (int i = 0; i < octaves; ++i) {
            value     += amplitude * noise2d(x * frequency, y * frequency);
            frequency *= lacunarity;
            amplitude *= gain;
        }
        return value;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  White / Blue / Pink / Brown Noise Generators
// ─────────────────────────────────────────────────────────────────────────────
class SpectralNoise {
    std::mt19937_64 rng;
public:
    explicit SpectralNoise(uint64_t seed = 42) : rng(seed) {}

    std::vector<double> white(int n) {
        std::normal_distribution<double> dist(0.0, 1.0);
        std::vector<double> out(n);
        for (auto& v : out) v = dist(rng);
        return out;
    }

    // Pink noise via Voss-McCartney algorithm
    std::vector<double> pink(int n) {
        std::normal_distribution<double> dist(0.0, 1.0);
        std::vector<double> out(n, 0.0);
        constexpr int ROWS = 16;
        double rows[ROWS] = {}, running_sum = 0.0;
        int cnt = 0;
        for (int i = 0; i < n; ++i) {
            int diff = cnt ^ (cnt + 1);
            cnt++;
            running_sum -= rows[0];
            rows[0] = dist(rng); running_sum += rows[0];
            for (int r = 1; r < ROWS && (diff >> r & 1); ++r) {
                running_sum -= rows[r];
                rows[r] = dist(rng);
                running_sum += rows[r];
            }
            out[i] = running_sum;
        }
        return out;
    }

    // Brown noise via integration of white
    std::vector<double> brown(int n) {
        auto w = white(n);
        std::vector<double> out(n);
        double cumsum = 0.0;
        for (int i = 0; i < n; ++i) { cumsum += w[i]; out[i] = cumsum; }
        // normalize
        double mn = *std::min_element(out.begin(), out.end());
        double mx = *std::max_element(out.begin(), out.end());
        double rng_ = mx - mn + 1e-12;
        for (auto& v : out) v = (v - mn) / rng_ * 2.0 - 1.0;
        return out;
    }

    // Blue noise via spectral inversion of pink
    std::vector<double> blue(int n) {
        auto p = pink(n);
        for (auto& v : p) v = -v;
        return p;
    }

    // Violet noise (f^2 shape)
    std::vector<double> violet(int n) {
        auto w = white(n);
        std::vector<double> out(n, 0.0);
        if (n < 2) return w;
        out[0] = w[0];
        for (int i = 1; i < n; ++i) out[i] = w[i] - w[i-1];
        return out;
    }

    // FFT-based power spectrum noise
    std::vector<double> powerSpectrum(int n, double exponent) {
        // Synthesize 1/f^alpha noise using spectral shaping
        int N = n;
        std::vector<std::complex<double>> spectrum(N, {0,0});
        std::normal_distribution<double> dist(0.0, 1.0);
        // Fill half-spectrum
        for (int k = 1; k < N/2; ++k) {
            double magnitude = std::pow(static_cast<double>(k), -exponent / 2.0);
            double phase     = dist(rng) * math::TAU;
            spectrum[k]      = {magnitude * std::cos(phase), magnitude * std::sin(phase)};
            spectrum[N - k]  = std::conj(spectrum[k]);
        }
        spectrum[0] = {0, 0};
        // IDFT (simple DFT inverse for correctness)
        std::vector<double> out(N, 0.0);
        for (int t = 0; t < N; ++t) {
            double sum = 0.0;
            for (int k = 0; k < N; ++k)
                sum += spectrum[k].real() * std::cos(2*math::PI*k*t/N)
                     - spectrum[k].imag() * std::sin(2*math::PI*k*t/N);
            out[t] = sum / N;
        }
        // normalize
        double mx = *std::max_element(out.begin(), out.end(), [](double a, double b){ return std::abs(a)<std::abs(b);});
        if (std::abs(mx) > 1e-12) for (auto& v : out) v /= std::abs(mx);
        return out;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Statistical & Signal Analysis
// ─────────────────────────────────────────────────────────────────────────────
struct NoiseStats {
    double mean, variance, std_dev, min_val, max_val, skewness, kurtosis;
    std::vector<double> histogram;
    std::vector<double> autocorrelation;
    std::vector<double> power_spectrum;
};

NoiseStats analyzeNoise(const std::vector<double>& data, int hist_bins = 64, int ac_lags = 64) {
    NoiseStats s{};
    int n = static_cast<int>(data.size());
    if (n == 0) return s;

    // Basic moments
    s.mean = std::accumulate(data.begin(), data.end(), 0.0) / n;
    double sq_sum = 0.0, cu_sum = 0.0, qu_sum = 0.0;
    s.min_val = data[0]; s.max_val = data[0];
    for (double v : data) {
        double d = v - s.mean;
        sq_sum += d * d; cu_sum += d * d * d; qu_sum += d * d * d * d;
        s.min_val = std::min(s.min_val, v); s.max_val = std::max(s.max_val, v);
    }
    s.variance = sq_sum / n;
    s.std_dev  = std::sqrt(s.variance);
    s.skewness = (s.std_dev < 1e-12) ? 0.0 : cu_sum / (n * s.std_dev * s.std_dev * s.std_dev);
    s.kurtosis = (s.variance < 1e-12) ? 0.0 : qu_sum / (n * s.variance * s.variance) - 3.0;

    // Histogram
    s.histogram.assign(hist_bins, 0.0);
    double range = s.max_val - s.min_val + 1e-12;
    for (double v : data) {
        int bin = static_cast<int>((v - s.min_val) / range * hist_bins);
        bin = std::clamp(bin, 0, hist_bins - 1);
        s.histogram[bin]++;
    }
    for (auto& h : s.histogram) h /= n;

    // Autocorrelation
    s.autocorrelation.assign(ac_lags, 0.0);
    for (int lag = 0; lag < ac_lags; ++lag) {
        double c = 0.0;
        for (int i = 0; i < n - lag; ++i)
            c += (data[i] - s.mean) * (data[i + lag] - s.mean);
        s.autocorrelation[lag] = c / (s.variance * (n - lag) + 1e-12);
    }

    // Power spectrum via DFT magnitude squared (first 256 bins or n/2)
    int fft_n = std::min(n, 512);
    s.power_spectrum.assign(fft_n / 2, 0.0);
    for (int k = 0; k < fft_n / 2; ++k) {
        double re = 0.0, im = 0.0;
        for (int t = 0; t < fft_n; ++t) {
            double angle = -2.0 * math::PI * k * t / fft_n;
            re += data[t % n] * std::cos(angle);
            im += data[t % n] * std::sin(angle);
        }
        s.power_spectrum[k] = (re*re + im*im) / fft_n;
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tile Generator: produces a flat RGBA8 image buffer
// ─────────────────────────────────────────────────────────────────────────────
enum class Algorithm {
    PERLIN_FBM, PERLIN_TURBULENCE, PERLIN_RIDGED, PERLIN_DOMAIN_WARP,
    SIMPLEX_FBM, SIMPLEX_TURBULENCE,
    WORLEY_F1, WORLEY_F2, WORLEY_F2_F1, WORLEY_CREASE,
    VALUE_FBM,
    WHITE, PINK, BROWN, BLUE, VIOLET
};

struct GeneratorParams {
    Algorithm algorithm        = Algorithm::PERLIN_FBM;
    int       width            = 512;
    int       height           = 512;
    double    scale            = 4.0;
    int       octaves          = 6;
    double    lacunarity       = 2.0;
    double    gain             = 0.5;
    double    warp_strength    = 1.5;
    uint64_t  seed             = 42;
    double    offset_x         = 0.0;
    double    offset_y         = 0.0;
    double    time_z           = 0.0;
    // Colormap: 0=grayscale 1=inferno 2=plasma 3=viridis 4=terrain 5=thermal 6=spectral 7=custom
    int       colormap         = 0;
    std::array<uint8_t, 3> custom_low  = {0, 0, 0};
    std::array<uint8_t, 3> custom_high = {255, 255, 255};
    bool      invert           = false;
    bool      normalize        = true;
    double    contrast         = 1.0;
    double    brightness       = 0.0;
};

// Colormap samplers
static std::array<uint8_t, 3> colormap_grayscale(double t) {
    uint8_t v = static_cast<uint8_t>(std::clamp(t * 255.0, 0.0, 255.0));
    return {v, v, v};
}
static std::array<uint8_t, 3> colormap_inferno(double t) {
    // Approximation via polynomial
    t = std::clamp(t, 0.0, 1.0);
    double r = std::clamp(3.54 * t - 0.34, 0.0, 1.0);
    double g = std::clamp(2.0  * t * t - 1.5 * t + 0.1, 0.0, 1.0);
    double b = std::clamp(std::sin(math::PI * t) * 0.8, 0.0, 1.0);
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}
static std::array<uint8_t, 3> colormap_plasma(double t) {
    t = std::clamp(t, 0.0, 1.0);
    double r = std::clamp(0.5 + std::sin(math::PI * (t - 0.5)) * 0.5 + t * 0.3, 0.0, 1.0);
    double g = std::clamp(t * t, 0.0, 1.0);
    double b = std::clamp(1.0 - 1.5 * t + t * t, 0.0, 1.0);
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}
static std::array<uint8_t, 3> colormap_viridis(double t) {
    t = std::clamp(t, 0.0, 1.0);
    double r = std::clamp(0.267 + 0.004 * t + 0.329 * t * t + 0.4  * t * t * t, 0.0, 1.0);
    double g = std::clamp(0.0   + 1.4 * t   - 1.4  * t * t  + 0.35 * t * t * t, 0.0, 1.0);
    double b = std::clamp(0.329 + 0.783 * t  - 2.1  * t * t  + 1.1  * t * t * t, 0.0, 1.0);
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}
static std::array<uint8_t, 3> colormap_terrain(double t) {
    t = std::clamp(t, 0.0, 1.0);
    if (t < 0.3) {
        double s = t / 0.3;
        return {0, static_cast<uint8_t>(100 + s * 80), static_cast<uint8_t>(180 - s * 80)};
    } else if (t < 0.6) {
        double s = (t - 0.3) / 0.3;
        return {static_cast<uint8_t>(s * 60), static_cast<uint8_t>(180 - s * 60), static_cast<uint8_t>(100 - s * 60)};
    } else if (t < 0.85) {
        double s = (t - 0.6) / 0.25;
        return {static_cast<uint8_t>(60 + s * 80), static_cast<uint8_t>(120 - s * 80), static_cast<uint8_t>(40 - s * 40)};
    } else {
        double s = (t - 0.85) / 0.15;
        return {static_cast<uint8_t>(140 + s * 115), static_cast<uint8_t>(40 + s * 215), static_cast<uint8_t>(40 + s * 215)};
    }
}
static std::array<uint8_t, 3> colormap_thermal(double t) {
    t = std::clamp(t, 0.0, 1.0);
    double r = std::clamp(3.0 * t - 1.5, 0.0, 1.0);
    double g = std::clamp(3.0 * std::abs(t - 0.5) - 0.5, 0.0, 1.0);
    double b = std::clamp(1.5 - 3.0 * t, 0.0, 1.0);
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}
static std::array<uint8_t, 3> colormap_spectral(double t) {
    t = std::clamp(t, 0.0, 1.0);
    double r = std::clamp(std::abs(t - 0.5) * 2.0, 0.0, 1.0);
    double g = std::clamp(std::sin(math::PI * t), 0.0, 1.0);
    double b = std::clamp(1.0 - t, 0.0, 1.0);
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main Generator
// ─────────────────────────────────────────────────────────────────────────────
class NoiseGenerator {
public:
    struct Result {
        std::vector<uint8_t>  rgba;        // width * height * 4
        std::vector<double>   raw;         // width * height (normalized [0,1])
        NoiseStats            stats;
        int                   width, height;
    };

    Result generate(const GeneratorParams& p) {
        int W = p.width, H = p.height;
        std::vector<double> buf(W * H);

        // Spectral 1D noise broadcast to 2D
        auto spectral_fill = [&](const std::vector<double>& sp) {
            int sz = static_cast<int>(sp.size());
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    buf[y * W + x] = sp[(x + y * W) % sz];
        };

        PerlinNoise  perlin(p.seed);
        SimplexNoise simplex(p.seed);
        WorleyNoise  worley(p.seed);
        ValueNoise   value(p.seed);
        SpectralNoise spectral(p.seed);

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                double nx = (p.offset_x + x) / W * p.scale;
                double ny = (p.offset_y + y) / H * p.scale;
                double nz = p.time_z;
                double v  = 0.0;
                switch (p.algorithm) {
                    case Algorithm::PERLIN_FBM:         v = perlin.fbm(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    case Algorithm::PERLIN_TURBULENCE:  v = perlin.turbulence(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    case Algorithm::PERLIN_RIDGED:      v = perlin.ridged(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    case Algorithm::PERLIN_DOMAIN_WARP: v = perlin.domainWarp(nx, ny, p.octaves, p.lacunarity, p.gain, p.warp_strength); break;
                    case Algorithm::SIMPLEX_FBM:        v = simplex.fbm(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    case Algorithm::SIMPLEX_TURBULENCE: v = simplex.turbulence(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    case Algorithm::WORLEY_F1:          v = worley.noise(nx, ny, WorleyNoise::Mode::F1); break;
                    case Algorithm::WORLEY_F2:          v = worley.noise(nx, ny, WorleyNoise::Mode::F2); break;
                    case Algorithm::WORLEY_F2_F1:       v = worley.noise(nx, ny, WorleyNoise::Mode::F2_F1); break;
                    case Algorithm::WORLEY_CREASE:      v = worley.noise(nx, ny, WorleyNoise::Mode::F1_F2_CREASE); break;
                    case Algorithm::VALUE_FBM:          v = value.fbm(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                    default:                            v = perlin.fbm(nx, ny, p.octaves, p.lacunarity, p.gain); break;
                }
                buf[y * W + x] = v;
            }
        }

        // Handle spectral algorithms
        if (p.algorithm == Algorithm::WHITE)  { spectral_fill(spectral.white(W * H)); }
        if (p.algorithm == Algorithm::PINK)   { spectral_fill(spectral.pink(W * H)); }
        if (p.algorithm == Algorithm::BROWN)  { spectral_fill(spectral.brown(W * H)); }
        if (p.algorithm == Algorithm::BLUE)   { spectral_fill(spectral.blue(W * H)); }
        if (p.algorithm == Algorithm::VIOLET) { spectral_fill(spectral.violet(W * H)); }

        // Normalize to [0,1]
        double mn = *std::min_element(buf.begin(), buf.end());
        double mx = *std::max_element(buf.begin(), buf.end());
        double rng = mx - mn + 1e-12;
        for (auto& v : buf) {
            v = (v - mn) / rng;
            if (p.invert) v = 1.0 - v;
            v = std::clamp(v * p.contrast + p.brightness, 0.0, 1.0);
        }

        // Stats
        NoiseStats stats = analyzeNoise(buf, 64, 64);

        // Colormap
        std::vector<uint8_t> rgba(W * H * 4);
        for (int i = 0; i < W * H; ++i) {
            double t = buf[i];
            std::array<uint8_t, 3> c{};
            switch (p.colormap) {
                case 0: c = colormap_grayscale(t); break;
                case 1: c = colormap_inferno(t);   break;
                case 2: c = colormap_plasma(t);    break;
                case 3: c = colormap_viridis(t);   break;
                case 4: c = colormap_terrain(t);   break;
                case 5: c = colormap_thermal(t);   break;
                case 6: c = colormap_spectral(t);  break;
                case 7: {
                    double s = t;
                    c = {static_cast<uint8_t>(p.custom_low[0] + s * (p.custom_high[0] - p.custom_low[0])),
                         static_cast<uint8_t>(p.custom_low[1] + s * (p.custom_high[1] - p.custom_low[1])),
                         static_cast<uint8_t>(p.custom_low[2] + s * (p.custom_high[2] - p.custom_low[2]))};
                    break;
                }
                default: c = colormap_grayscale(t); break;
            }
            rgba[i * 4]     = c[0];
            rgba[i * 4 + 1] = c[1];
            rgba[i * 4 + 2] = c[2];
            rgba[i * 4 + 3] = 255;
        }

        return {std::move(rgba), std::move(buf), std::move(stats), W, H};
    }
};

} // namespace noise
