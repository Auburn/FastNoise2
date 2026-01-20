[![GitHub Actions CI](https://img.shields.io/github/actions/workflow/status/Auburn/FastNoise2/main.yml?branch=master&style=for-the-badge&logo=GitHub "GitHub Actions CI")](https://github.com/Auburn/FastNoise2/actions?query=workflow%3ACI)
[![Discord](https://img.shields.io/discord/703636892901441577?style=for-the-badge&logo=discord "Discord")](https://discord.gg/SHVaVfV)
[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)

# FastNoise2

Modular node based noise generation library using SIMD, focused on performance, modern C++17 and designed with ease of use in mind.

Noise node graphs can be created in code or with the help of the included visual "Node Editor" tool. Or if you just want basic coherent noise you can easily generate it from a single Simplex/Perlin node

### Why Nodes?

The node-based approach keeps all noise generation and operations (add, multiply, blend, etc.) within the SIMD pipeline. This means when combining multiple noise types or applying modifiers, the intermediate values stay in SIMD registers rather than being written to memory.

Traditional approaches require allocating separate arrays for each noise type, generating them individually, then combining them afterwards with scalar or less efficient operations. With FastNoise2's node graph, the entire computation is fused and executed in SIMD, maximizing throughput and minimizing both memory allocation and bandwidth.

## Features

**Coherent Noise**
- Perlin, Simplex, SuperSimplex (OpenSimplex2S), Value
- Cellular Value, Cellular Distance, Cellular Lookup

**Fractals**
- FBm, Ridged

**Blends & Operators**
- Add, Subtract, Multiply, Divide, (Smooth)Min, (Smooth)Max, Fade, and more

**Modifiers**
- Remap, Terrace, Domain Scale/Offset/Rotate, and more

**Domain Warping**
- Gradient, Simplex, SuperSimplex
- Fractal Progressive, Fractal Independent

**Dimensions**
- 2D, 3D, 4D noise generation + 2D tiling support

**Thread Safety**
- Fully thread-safe: generate noise in parallel across multiple threads with the same node tree

**Serialization**
- Node trees can be encoded to compact strings and decoded at runtime for quick iteration
- Create complex noise setups in the Node Editor and load them directly into your application
- Connect your application directly to the node editor and see live node tree update in your engine

## Node Editor

The FastNoise2 Node Editor tool provides a node graph editor to create trees of FastNoise2 nodes. Node trees can be exported as serialised strings and loaded into the FastNoise2 library in your own code. Node Editor has 2D texture/heightmap and 3D mesh previews for the node graph output, generation is infinite in all dimensions. See screenshots below for examples.

Live web version WASM, [Web Node Editor](https://auburn.github.io/fastnoise2nodeeditor/)

Check the [Releases](https://github.com/Auburn/FastNoise2/releases/latest) for compiled Node Editor binaries

![Node Editor Mountain Terrain](https://github.com/user-attachments/assets/acda59bd-1245-40dd-8f0e-06fa8a17c60d)

![Node Editor Crazy Terrain](https://github.com/user-attachments/assets/22bb2052-b17d-415e-897d-999869991fa9)

## FastNoise2 vs FastNoise Lite

[FastNoise Lite](https://github.com/Auburn/FastNoiseLite) is a simpler, portable library best suited for basic noise needs in many languages. Choose **FastNoise2** when you need:
- Maximum performance through SIMD optimization
- Complex noise combinations without performance penalties
- Domain warping and advanced modifiers
- Runtime-configurable node graphs

## Platform Support

Uses [FastSIMD](https://github.com/Auburn/FastSIMD) to compile code with multiple SIMD architectures and selects the fastest supported SIMD level at runtime
- Scalar (non-SIMD)
- SSE2
- SSE4.1
- AVX2
- AVX512
- NEON
- WASM SIMD

Supports:
- 32/64 bit
- Windows
- Linux
- Android
- MacOS x86/ARM
- iOS
- MSVC
- Clang
- GCC
- Emscripten (WASM)

Bindings:
- [C#](https://github.com/Auburn/FastNoise2Bindings)
- [Unreal Engine CMake](https://github.com/caseymcc/UE4_FastNoise2)
- [Unreal Engine Blueprint](https://github.com/DoubleDeez/UnrealFastNoise2)
- [Rust](https://github.com/Lemonzyy/fastnoise2-rs)
- [Java](https://github.com/CoolLoong/FastNoise2Bindings-Java)

Roadmap:
- [Vague collection of ideas](https://github.com/users/Auburn/projects/1)

## Performance

FastNoise2 has continuous benchmarking to track of performance for each node type across commits

Results can be found here: https://auburn.github.io/fastnoise2benchmarking/

### Library Comparisons

Benchmarked using [NoiseBenchmarking](https://github.com/Auburn/NoiseBenchmarking)

- CPU: Intel 7820X @ 4.9Ghz
- OS: Win10 x64
- Compiler: clang-cl 10.0.0 -m64 /O2

Million points of noise generated per second (higher = better)

| 3D                 | Value  | Perlin | (*Open)Simplex | Cellular |
|--------------------|--------|--------|----------------|----------|
| FastNoise Lite     | 64.13  | 47.93  | 36.83*         | 12.49    |
| FastNoise (Legacy) | 49.34  | 37.75  | 44.74          | 13.27    |
| FastNoise2 (AVX2)  | 494.49 | 261.10 | 268.44         | 52.43    |
| libnoise           |        | 27.35  |                | 0.65     |
| stb perlin         |        | 34.32  |                |          |

| 2D                 | Value  | Perlin | Simplex | Cellular |
|--------------------|--------|--------|---------|----------|
| FastNoise Lite     | 114.01 | 92.83  | 71.30   | 39.15    |
| FastNoise (Legacy) | 102.12 | 87.99  | 65.29   | 36.84    |
| FastNoise2 (AVX2)  | 776.33 | 624.27 | 466.03  | 194.30   |

# Getting Started

See [Wiki](https://github.com/Auburn/FastNoise2/wiki)
