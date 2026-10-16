# CodeGen internals

`FastNoise::CodeGen` (`include/FastNoise/CodeGen.h`) turns a node tree into standalone GLSL, HLSL, WGSL or FastSIMD C++.
It works by tracing: the existing node implementations are run once on symbolic registers, so there is no second implementation of any node to maintain.

## How it works

Node implementations (`include/FastNoise/Generators/*.inl`) are written against `float32v`/`int32v`/`mask32v` and `FS::` functions, and compiled once per SIMD feature set through `FastSIMD_Build.inl`.
`Trace.cpp` compiles the same `FastSIMD_Build.inl` one more time with `TraceToolSet.h` standing in for FastSIMD's ToolSet.
There the register types are symbolic: every operation appends an instruction to an SSA function (`IR.h`) instead of computing a value.

- The trace feature set (`FastSIMD::kTraceFeatureSet`) has no feature flags, so every SIMD specific branch takes its generic path.
- `Trace.cpp` provides its own `DispatchClass<Generator, kTraceFeatureSet>`, its `GetSourceValue()` records a call to the source node rather than making a virtual call.
- Node settings are plain C++ values while tracing, so they become constants, and loops over octaves/cells are unrolled.
- Trace nodes are created from `NodeData` through the metadata setters, the same as decoding an encoded node tree.

Each (node, dimension count) pair becomes a function.
A two pass trace inlines functions that have a single call site in the final program, or that are small, into their caller.
Everything else stays a function, for example the source of a fractal which is called once per octave.

`IR::Builder` hash conses instructions (common subexpressions, including identical source calls), folds constants and applies simplifications.
Constant folding and the reference interpreter (`Program::Evaluate()`) share `EvaluateOp()`, which reproduces FastSIMD's non relaxed semantics exactly, including its polynomial `Sin`/`Cos`/`Exp`/`Log`.

`Emit.cpp` writes the program in each language, single use expressions are written inline and everything else into a variable.

## Rules for node implementations

New nodes registered in `FastSIMD_Build.inl` are traced automatically. Tracing only sees values that flow through the register types, so:

- Control flow can depend on node settings but not on register values.
  `FS::AnyMask()` always returns true when tracing, it may only be used to skip work that would be masked out anyway (as in `Terrace`).
- Memory accessed through `FS::Load`/`FS::Store` is not tracked. `GeneratorCache` relies on this to trace as a pass through.
- `FS::Constant` and `FS::NativeExec` (per lane values, native intrinsics) may only appear in SIMD specific branches that have a generic fallback.
- Sources must be sampled through `GetSourceValue()`/`GetSourceSIMD()`.

## Exactness

- Integer ops and hashing are reproduced exactly in every language.
- `Options::fastMath` (default on) allows simplifications that only differ for -0, inf and NaN, for example `x * 0 -> 0` and `x < inf -> true`.
- A float select whose result is used as bits (`Masked( m, -0.0f )` style sign flips) is emitted as an integer select.
  Fast math C++ compilers and GPU drivers don't preserve signed zero through float selects.
- GPU output uses native `sqrt`, `inversesqrt`, `sin`, `cos` and `pow`, so float results differ slightly from the CPU.
  Discontinuous nodes (cellular value, white noise) can amplify this near edges.

## Validation

- `FastNoiseCodeGenTest` compares traced programs with CPU output for every node type (random settings, sources driven by coherent noise),
  the demo node trees and random node trees, in 2D/3D/4D. In a `FASTNOISE2_STRICT_FP` build the reference evaluation must match the
  minimum compiled feature set exactly. `FastNoiseCodeGenTest <count>` sets the number of random trees.
- `CodeGenGpuCheck.py` runs the cases exported by `FastNoiseCodeGenTest --export cases.jsonl` as GLSL, WGSL and HLSL compute shaders on a GPU.
  Mesa's llvmpipe (OpenGL) and lavapipe (Vulkan) work headless.
- `FastNoiseCodeGenBenchmark` compiles generated C++ for a set of node trees into a dispatch library and benchmarks it against the runtime trees.

Some feature sets already disagree with each other, which the tests account for:
the AVX512 4D gradient dot products sum in a different order to the generic path (1 ulp, even with `FASTNOISE2_STRICT_FP`),
and SSE2 `FS::Round` returns +0 for inputs in (-0.5, 0) where SSE4.1 returns -0.
