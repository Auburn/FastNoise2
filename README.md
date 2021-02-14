[![GitHub Actions CI](https://img.shields.io/github/workflow/status/Auburn/FastNoise2/CI?style=flat-square&logo=GitHub "GitHub Actions CI")](https://github.com/Auburn/FastNoise2/actions)
[![Discord](https://img.shields.io/discord/703636892901441577?style=flat-square&logo=discord "Discord")](https://discord.gg/SHVaVfV)

# FastNoise2

WIP successor to [FastNoiseSIMD](https://github.com/Auburn/FastNoiseSIMD)

FastNoise2 is a fully featured noise generation library which aims to meet all your coherent noise needs while being extremely fast

Uses FastSIMD to compile classes with multiple SIMD types

Supports:
- 32/64 bit
- Windows
- Linux
- MacOS
- MSVC
- Clang
- GCC

Check the [releases](https://github.com/Auburn/FastNoise2/releases) for early versions of the Noise Tool

![NoiseTool](https://user-images.githubusercontent.com/1349548/90967950-4e8da600-e4de-11ea-902a-94e72cb86481.png)

## Performance Comparisons

Benchmarked using C++ version with [NoiseBenchmarking](https://github.com/Auburn/NoiseBenchmarking)

- CPU: Intel 7820X @ 4.9Ghz
- OS: Win10 x64
- Compiler: clang-cl 10.0.0 -m64 /O2

Million points of noise generated per second (higher = better)

| 3D                 | Value  | Perlin | (*Open)Simplex | Cellular |
|--------------------|--------|--------|----------------|----------|
| FastNoise Lite     | 64.13  | 47.93  | 36.83*         | 12.49    |
| FastNoise (Legacy) | 49.34  | 37.75  | 44.74          | 13.27    |
| FastNoise 2 (AVX2) | 494.49 | 261.10 | 268.44         | 52.43    |
| libnoise           |        | 27.35  |                | 0.65     |
| stb perlin         |        | 34.32  |                |          |

| 2D                 | Value  | Perlin | Simplex | Cellular |
|--------------------|--------|--------|---------|----------|
| FastNoise Lite     | 114.01 | 92.83  | 71.30   | 39.15    |
| FastNoise (Legacy) | 102.12 | 87.99  | 65.29   | 36.84    |
| FastNoise 2 (AVX2) | 776.33 | 624.27 | 466.03  | 194.30   |

# Getting Started

See [documentation](https://github.com/Auburn/FastNoise2/wiki)
