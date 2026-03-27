"""
setup.py — build noise_engine C++ extension via pybind11

Usage:
  python setup.py build_ext --inplace      # build next to source files
  pip install .                             # install into site-packages
  pip install -e .                          # editable / development install
"""

import sys
import os
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

# ── Locate pybind11 include path robustly ────────────────────────────────────
try:
    import pybind11
    PYBIND11_INCLUDE = pybind11.get_include()
except ImportError:
    sys.exit(
        "pybind11 is required to build noise_engine.\n"
        "Install it with:  pip install pybind11"
    )

# ── Compiler flag helpers ─────────────────────────────────────────────────────
class BuildExt(build_ext):
    """Inject platform-specific compile / link flags."""

    _compile_flags = {
        "unix": [
            "-O3",
            "-march=native",
            "-ffast-math",
            "-funroll-loops",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wno-unused-parameter",
        ],
        "msvc": [
            "/O2",
            "/std:c++17",
            "/W3",
            "/EHsc",
            "/arch:AVX2",   # remove if targeting older hardware
        ],
    }

    def build_extensions(self):
        ct = self.compiler.compiler_type          # 'unix' or 'msvc'
        flags = self._compile_flags.get(ct, [])

        for ext in self.extensions:
            ext.extra_compile_args = flags

            if ct == "unix":
                # Link-time optimisation on Linux/macOS
                ext.extra_link_args = ["-flto"]
            elif ct == "msvc":
                ext.extra_link_args = ["/LTCG"]

        super().build_extensions()


# ── Extension definition ──────────────────────────────────────────────────────
ext = Extension(
    name="noise_engine",
    sources=["bindings.cpp"],
    include_dirs=[
        PYBIND11_INCLUDE,
        os.path.dirname(os.path.abspath(__file__)),  # for noise_engine.hpp
    ],
    language="c++",
)

# ── setup() ───────────────────────────────────────────────────────────────────
setup(
    name="noise_engine",
    version="2.0.0",
    author="Noise Visualizer",
    description="Enterprise stochastic noise generation engine (C++ / pybind11)",
    long_description=open("README.md", encoding="utf-8").read() if os.path.exists("README.md") else "",
    ext_modules=[ext],
    cmdclass={"build_ext": BuildExt},
    python_requires=">=3.9",
    install_requires=["pybind11>=2.10"],
    zip_safe=False,
)
