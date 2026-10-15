# Node tree code generation: shader export and fused CPU evaluation

Investigation into generating shader code from FastNoise2 node trees, and into
removing the virtual `Gen` calls between nodes on the CPU. Includes measurements,
a comparison of the options, and a working proof of concept that lives on this branch
(`include/FastNoise/Codegen.h`, `src/FastNoise/Codegen/`, `util/NoiseCodegen/`).

## Summary

**Recommendation: trace the existing node code with a symbolic register type.**
Every node implementation is already written against `FS::Register<T, N, SIMD>` and the
`FS::` function set. Specialising those templates for a pseudo feature set that no hardware
backend uses lets the *unmodified* node code run once per node and dimension count with
registers that record an expression graph instead of computing values. That graph is then
printed as GLSL, HLSL, WGSL, scalar C++ or (follow-up) FastSIMD C++.

Why this is the elegant option:

- **No change to any node.** The proof of concept instantiates all 47 node types against the
  symbolic registers with zero edits to `Generators/*.inl` or `Generator.inl`. A node added
  tomorrow gets shader output for free, and the shader can never drift from the CPU
  implementation because it *is* the CPU implementation.
- **One mechanism for both goals.** The same IR printed as FastSIMD C++ is a fully fused,
  virtual-call-free "baked tree" node, which is the only form of CPU devirtualisation that
  measurably pays off (see below).
- **Small footprint.** About 900 lines of new code (IR, symbolic registers, replacement base
  class, emitters), one three-line addition to FastSIMD for a dedicated feature set value, and
  a two-line CMake change for the command line utility.

**On skipping the virtual calls alone: not worth a structural change.** Each virtual `Gen`
call is amortised over `kRegisterSize` lanes (16 on AVX2, 32 on AVX-512). Measured on this
machine, a direct (devirtualised but not inlined) source call gives 0 to 9 percent on
FBm-of-Simplex, and per-node dispatch is about 4 to 5 percent of the two demo trees. Inlining
the source body into the fractal loop gives 10 to 37 percent, so the gain comes from
cross-node optimisation (constant propagation, shared position scaling, no ABI boundary),
which a runtime tree cannot get without generating code for the whole tree.

**Validation of the proof of concept.** All 47 node types instantiate against the symbolic
registers with no change to node code. 64 trees (the two Node Editor demo trees, FBm of
Simplex, one default-constructed tree per node type and 12 hand-built combinations) were
generated in all four languages and all three dimension counts: every one of the 192 GLSL and
192 HLSL files compiles with glslang. The scalar C++ output, compiled with IEEE semantics and
compared with the library's `GenSingle*` on 20000 random points per tree and dimension count,
is bit-exact for 153 of the 168 tree and dimension pairs against the strict build
(`FASTNOISE2_STRICT_FP=ON`), within 2e-6 for 11 more, and within 5e-3 for the three that go
through FastSIMD's polynomial `pow` and `sin`. Against the default relaxed build the typical
difference is 1e-5, from FMA contraction and the approximate reciprocal and inverse square
root. Generating a demo tree takes 0.1 to 0.7 ms.

The validation also uncovered an existing library bug: with clang on AVX2 and AVX-512,
`AddDimension` evaluated in 3D hands a wrong fourth coordinate to its 4D source
(see "Bug found on the way").

## 1. How a tree is evaluated today

`Generator` is the public interface. For each compiled SIMD feature set,
`FastSIMD::DispatchClass<Node, SIMD>` is a `final` class whose `Gen( seed, x, y[, z[, w]] )`
overrides are virtual and take `kRegisterSize`-wide registers (twice the native width). A node
reads its inputs through `GetSourceValue( source, seed, pos... )`, which is an indirect call
`simdGeneratorPtr->Gen( seed, pos... )` into the source node. So every node boundary costs one
virtual call per batch of 8 to 32 samples plus whatever the ABI boundary prevents the compiler
from optimising across (broadcasts of member values, repeated `ScalePositions`, register
spills for the 4D signature).

Everything a node computes goes through `float32v`, `int32v`, `mask32v` and about 45 `FS::`
functions. Scalar control flow in node code only depends on member values (`mOctaves`,
`mDistanceFunction`, `mClampOutput`, pointer-is-set checks) or `if constexpr`; the only
lane-dependent branch is `FS::AnyMask` in `Terrace`, which guards work whose result is masked
anyway. This is what makes tracing possible.

## 2. Measurements

Scratch harness (not committed): FastNoise2 built with clang 18, `-O3 -ffast-math`, Intel Xeon
2.8 GHz with AVX-512, `GenPositionArray` over 8192 positions, best of 9 runs. Numbers are
nanoseconds per sample for 2D / 3D / 4D.

| Tree | AVX2 | AVX-512 |
|---|---|---|
| Simplex alone | 2.27 / 4.89 / 6.75 | 1.25 / 2.09 / 4.50 |
| FBm(Simplex) 5 octaves, stock (virtual source call) | 13.41 / 29.79 / 37.60 | 8.26 / 12.34 / 23.06 |
| same, source devirtualised (direct call, not inlined) | 12.94 / 29.56 / 37.52 | 7.56 / 11.81 / 23.19 |
| same, source body force-inlined into the octave loop | 10.78 / 21.74 / 32.16 | 6.58 / 11.29 / 18.31 |
| cost per extra `Add` node in a chain (4 to 16 nodes) | 0.20 / 0.23 / 0.31 | 0.16 / 0.20 / 0.24 |
| Mountain Terrain demo tree (7 nodes) | 66.8 / 74.0 / 173.3 | 35.6 / 37.8 / 103.7 |
| Cellular Caves demo tree (12 nodes) | 53.7 / 84.8 / 268.3 | 36.6 / 55.0 / 177.5 |

Speedups relative to the stock tree: devirtualised 1.04 / 1.01 / 1.00 (AVX2) and
1.09 / 1.04 / 0.99 (AVX-512); inlined 1.24 / 1.37 / 1.17 (AVX2) and 1.26 / 1.09 / 1.26
(AVX-512). On SSE2 and SSE4.1 the inlined variant was within noise or slightly slower,
presumably from register pressure.

The devirtualised variant was a copy of `FractalFBm` with `GeneratorSourceT<Simplex>` as its
source type calling `Gen` through the `final` class pointer; the inlined variant additionally
marked `Simplex::Gen` `FS_FORCEINLINE` (and `public`). Both would be needed for a
"typed source" devirtualisation in the real library.

Per-node dispatch share of a realistic tree: Mountain Terrain has 7 nodes at roughly
0.2 ns each out of 37.8 ns (AVX-512 3D), about 4 percent; Cellular Caves 12 nodes, about
4 percent. The chain measurement grows to 0.4 to 0.8 ns per node at 64 nodes deep, where the
call depth itself starts to cost, but no real tree is that deep.

## 3. Options

### A. Symbolic tracing of the existing node code (recommended, prototyped)

Instantiate `DispatchClass<Node, kSymbolic>` for a feature set value with no hardware flags,
with `FS::Register<float|int32_t|Mask, N, kSymbolic>` specialised to append instructions to a
builder and carry an instruction index. `GetSourceValue` becomes a `Call` instruction and
schedules the callee for tracing, so each node instance becomes one function per dimension
count. Loops over `mOctaves` unroll into repeated calls, `switch`es on enums collapse to the
selected case, hybrid sources without a node become constants, unused code never appears.

What it needs from the existing code:

- Nothing in the node sources. The constraints node code must keep (all already met) are
  listed in section 5.
- A feature set value for the symbolic instantiation. The prototype reuses
  `FastSIMD::FeatureSet::Invalid` (no flags, never instantiated by a real backend) through two
  translation-unit-local macros. The proper version is a dedicated enumerator in FastSIMD,
  roughly: `FeatureFlag::Symbolic`, `FeatureSet::SYMBOLIC = Invalid | FeatureFlag::Symbolic`,
  `#define FASTSIMD_FEATURE_VALUE_SYMBOLIC() 1`, and a string case. A distinct value is not
  cosmetic: the symbolic `DispatchClass<Simplex, X>` must be a different type from every
  hardware instantiation so that the two definitions never collide under the one definition
  rule when both are linked into the same program.
- The batch-loop base `DispatchClass<Generator, SIMD>` from `Generator.inl` is replaced by an
  explicit specialisation for the symbolic feature set that stubs the batch API. The stock
  partial specialisation still gets type-checked in the codegen unit (its `float32v` is not a
  dependent type), which only required `Extract0`, `Load`, `Store`, `Min` and `Max` overloads
  on the symbolic types; the code is never instantiated.
- The codegen unit is compiled with `FASTSIMD_MAX_FEATURE_SET=SCALAR` so that no x86 register
  headers are included; their helper overloads accept any register that declares `NativeType`
  and would otherwise be ambiguous with the symbolic ones.

### B. Hand-written shader implementations of every node plus metadata-driven glue

A `.glsl` library with one function per node, and a generator that walks `NodeData` to emit
the glue (argument wiring, constants). This is the conventional approach and needs no C++
template work, but it means 47 nodes times 3 dimensions of GLSL (and HLSL, and WGSL) to write
and keep in sync with the SIMD code by hand. Every bug fix or new node is a second
implementation, and nothing stops the two from diverging silently. The metadata gives the
structure but not the maths. Rejected for maintenance cost; option A delivers the same output
from the one implementation.

### C. Devirtualising node calls on the CPU without code generation

- *Typed sources.* `GeneratorSourceT<Simplex>` instead of `GeneratorSource` lets
  `GetSourceValue` call `Gen` on the `final` class, which the compiler devirtualises. It needs
  the node `Gen` functions to be `public` (they are private today) and, to be useful, a node
  variant per source type (`FractalFBm<Simplex>` and so on), i.e. new node ids. Measured
  benefit without inlining: 0 to 9 percent.
- *Fused hot pairs.* The same, plus forcing the source body inline. Measured: 10 to 37 percent
  on fractal-of-noise, but only for the specific pairs that get a fused class, with the
  combinatorial cost that implies (3 fractal types times 6 leaf types times 3 dimensions of
  instantiated Simplex-sized bodies per feature set), and no benefit for the operator and
  modifier nodes that make up most of a real tree.
- *Runtime JIT* (LLVM ORC, asmjit, Cranelift). Gives the full fusion for runtime trees but adds
  a large dependency to a library that is meant to be small and embeddable, and it duplicates
  the work of tracing anyway (the JIT also needs the tree as an expression graph).

Recommendation: do not pursue the first two as library changes. For shipped content, emit
FastSIMD C++ from the traced IR (option A, section 6) and compile it as a custom node; that
removes every virtual call and lets the compiler see the whole tree.

## 4. The proof of concept on this branch

Files:

| File | Purpose |
|---|---|
| `include/FastNoise/Codegen.h` | Public API: `Codegen::Generate( encodedTree, Options )` and the `NodeData` overload. `Options` selects language, dimension count, entry name and fma emission. |
| `src/FastNoise/Codegen/CodegenIR.h` | SSA instruction set (about 60 ops), `Function`, `Module`, and the `Builder` with hash-consing and constant folding. |
| `src/FastNoise/Codegen/CodegenRegister.h` | The symbolic `FS::Register` specialisations and the `FS::` primitives on them. Everything FastSIMD can compose from primitives (`MaskedAdd`, `SignBit`, `Reciprocal`, ...) is left to FastSIMD's generic code. |
| `src/FastNoise/Codegen/CodegenGenerator.h` | Replacement `DispatchClass<Generator, symbolic>`: `Gen` virtuals, `GetSourceValue` emitting `Call`, stubbed batch API. |
| `src/FastNoise/Codegen/Codegen.cpp` | The instantiation unit: includes `FastSIMD_Build.inl` with a redefined `FASTNOISE_REGISTER_NODE` that explicitly instantiates each node and registers a factory; builds symbolic nodes from `NodeData` through the existing metadata setters; the tracer. |
| `src/FastNoise/Codegen/CodegenEmit.cpp` | GLSL / HLSL / WGSL / scalar C++ printers. |
| `src/FastNoise/Codegen/CodegenIR.cpp` | Out-of-line `Builder::Emit` and the constant folder (keeping these out of line halves the instantiated code). |
| `util/NoiseCodegen/` | Command line tool: `NoiseCodegen <encoded tree> [glsl|hlsl|wgsl|cpp] [2|3|4] [entry]`. Build with `-DFASTNOISE2_UTILITY=ON`. |
| `src/CMakeLists.txt`, `util/CMakeLists.txt` | `-Os` for the codegen unit; the utility subdirectory. The only existing files touched. |

Usage:

```cpp
FastNoise::Codegen::Options options;
options.language = FastNoise::Codegen::Language::GLSL;
options.dimensions = 3;
std::string glsl = FastNoise::Codegen::Generate( "DQkGDA==", options );
```

Output for that tree (FBm of Simplex, 3D) after the Simplex function, which is 100 lines of
straight-line code:

```glsl
float fn_FractalFBm_0_3D(int seed, float x, float y, float z)
{
    float t8 = fn_Simplex_1_3D(seed, x, y, z);
    int t11 = seed + 1;
    float t15 = x * 2.0;
    float t16 = y * 2.0;
    float t17 = z * 2.0;
    float t18 = fn_Simplex_1_3D(t11, t15, t16, t17);
    float t29 = fn_Simplex_1_3D((t11 + 1), (t15 * 2.0), (t16 * 2.0), (t17 * 2.0));
    return (t8 + (t18 * 0.5)) + (t29 * 0.25);
}

float FastNoise_Gen(int seed, float x, float y, float z)
{
    return fn_FractalFBm_0_3D(seed, x, y, z);
}

float FastNoise_Gen(int seed, vec3 pos)
{
    return fn_FractalFBm_0_3D(seed, pos.x, pos.y, pos.z);
}
```

The default gain of 0.5 and weighted strength of 0 have been folded into the octave weights;
the three octaves are calls, not copies.

Output characteristics:

- One function per node instance and dimension count, named `fn_<Node>_<index>_<dims>D`.
  Nodes referenced from several places in the tree are emitted once. Fractal octaves become
  repeated calls, so a 16 octave fractal is 16 call lines, not 16 copies of the noise body.
  `DomainWarpFractal` is the exception: its `Warp` calls are direct C++ calls in the node
  code, so the warp body is traced inline once per octave.
- Member values are baked as constants; a tree with different parameters is a different
  shader. Common sub-expressions are shared per function, dead code (for example the
  unselected `switch` cases and `GeneratorCache` bookkeeping) is dropped, and single-use
  temporaries are inlined up to a nesting depth of 6.
- Integer arithmetic that must wrap (the hashes) is emitted through unsigned types,
  float to int conversion uses round-to-nearest-even like the SIMD backends
  (`roundEven` in GLSL, `round` in WGSL, a helper in HLSL), float bit tricks use
  `floatBitsToInt` and friends, `pow` keeps FastSIMD's "0 for non-positive base" rule.
- Line counts for 3D GLSL: FBm of Simplex 116, Mountain Terrain 477, Cellular Caves 577, a
  four octave progressive domain warp fractal 410; a single 4D SuperSimplex is 363.

Tracing contract for node authors (everything in the current node set already satisfies it):

1. All lane math goes through `float32v`, `int32v`, `mask32v` and `FS::` (already the rule).
2. Scalar branches and loop bounds depend only on member values, `sizeof...( P )` or
   `if constexpr`. The one allowed lane-dependent test is `FS::AnyMask`, and only to skip work
   whose result is masked out anyway (`Terrace`); the tracer answers "maybe" to it.
3. No loads or stores of lane data. `GeneratorCache`'s thread-local cache is the only case; its
   loads trace to undefined values that end up dead and its stores are dropped, so the node
   becomes a pass-through.
4. Hardware intrinsics stay inside `if constexpr( SIMD & Flag )`, which they already do.
5. Registers read before being written (`cellX` in `CellularLookup`, `valueHash` in
   `CellularValue`) print as zero. They only feed the unselected side of a `Select`.

Validation performed (scratch tools, not committed; see follow-up 4 for making them tests):

- A generator tool builds the 64 trees as `NodeData`, serialises them, and calls `Generate`
  for GLSL, HLSL, WGSL and scalar C++ in 2D, 3D and 4D. Each GLSL file is wrapped in a
  `#version 450` fragment shader and each HLSL file in a pixel shader and run through
  `glslangValidator` (HLSL via SPIR-V generation): 192 of 192 pass for both.
- A verifier links the three scalar C++ files of a tree (compiled with
  `-fno-fast-math -ffp-contract=off`) against the library and compares with
  `GenSingle2D/3D/4D` at 20000 random positions in ±2000 with random seeds, per dimension
  count.

| Library build | Tree and dimension pairs | Bit-exact | Max difference < 1e-6 | < 1e-4 | < 1e-2 | Larger |
|---|---|---|---|---|---|---|
| strict, excluding the 7 trees hit by the bug below | 168 | 153 | 11 | 1 | 3 | 0 |
| relaxed, excluding the 8 trees hit by the bug below | 165 | 117 | 10 | 31 | 1 | 6 |

The strict residuals are 4D noise (1 to 2 ulp, cause not chased), `SineWave` (5e-7) and the
three `PowFloat`/Minkowski/`SineWave` combinations (5e-3, polynomial approximations). The
relaxed residuals above 1e-2 are `DistanceToPoint` (values around 1e6, so 1e-6 relative) and
the same approximation trees; cellular trees sit at 1e-4 from the 12-bit `rsqrt`.

### Bug found on the way

`AddDimension` evaluated in 3D passes `( x, y, z, newDimension )` to its source. With clang 18
on Linux, the AVX2 and AVX-512 builds hand the source a fourth coordinate of 0 when the new
dimension is a constant, and garbage (observed `-inf`) when it comes from a node; SSE2 and
SSE4.1 builds and the 2D to 3D case are correct. Reproduction without any of the new code:

```cpp
auto grad = FastNoise::New<FastNoise::Gradient>( level ); // outputs w when W multiplier is 1
grad->SetMultiplier<FastNoise::Dim::W>( 1.0f );
auto add = FastNoise::New<FastNoise::AddDimension>( level );
add->SetSource( grad ); add->SetNewDimensionPosition( 0.37f );
add->GenSingle3D( 1, 2, 3, 0 ); // 0.37 on SSE2/SSE4.1, 0 on AVX2/AVX512
```

Isolation: the same library built with GCC 13, or with clang 18 after defining
`FS_VECTORCALL` empty in FastSIMD's `Register.h`, returns 0.37 on every feature set. So the
fault is in clang's `__regcall` convention (FastSIMD uses it for every `Gen`) when the 4D
signature's five two-register arguments are passed through the virtual call from
`AddDimension::GenT`; the standalone 4D path through `GenSingle4D` is fine.

A second manifestation of the same thing: `CellularLookup` evaluated in 4D (its lookup call
passes four computed cell coordinates to the source) returns NaN for every sample on AVX2 and
AVX-512 in the default relaxed build, and is correct on SSE2, SSE4.1 and in the strict build,
where the different register allocation happens not to trigger it. The generated code agreed
with the correct variants in both cases, which is how the two were noticed.

It affects the clang builds in CI (Linux and macOS) on AVX2 and AVX-512 hardware for any tree
that contains `AddDimension` evaluated in 3D or `CellularLookup` evaluated in 4D. It is
independent of this investigation and is not fixed on this branch; it is worth a bug report
against FastSIMD (or a `FS_VECTORCALL` opt-out for the 4D overloads) in its own right.

## 5. Semantics to keep in mind for GPU output

- The library's default build is `-ffast-math` with FMA contraction and uses `rcpps` /
  `rsqrtps` for `Reciprocal` and `InvSqrt` (about 12 bits of precision). Generated code uses
  exact division and `inversesqrt`, so results differ from the relaxed CPU build at the 1e-5
  level in general and more where the approximations are amplified (cellular distances,
  smooth min/max). Against the strict build (`FASTNOISE2_STRICT_FP=ON`) the scalar C++
  output is bit-exact in 2D and 3D.
- `Exp`, `Log`, `Pow`, `Sin`, `Cos` are polynomial approximations in FastSIMD but builtins in
  the generated code; `PowFloat`, `SineWave`, Minkowski distances and `GenTileable2D`
  therefore differ by the approximation error.
- GPU compilers contract `a * b + c` into fma by default; `Options::emitFma` makes it explicit.
- Denormals: the relaxed build runs with flush-to-zero (crtfastmath); GPUs also flush. Not an
  issue in practice.
- Coordinates beyond about 2^24 lose precision identically on both sides.

## 6. Follow-ups, in suggested order

1. **FastSIMD: dedicated symbolic feature set** (three lines, see option A). Then drop the
   `Invalid` macros in `Codegen.cpp`.
2. **Decide the module's place**: always in the library or behind a CMake option. The
   codegen unit instantiates every node once more; compiled at `-Os` (set per source in
   `src/CMakeLists.txt`, since tracing speed does not matter) it is about 0.7 MB of code and
   takes 10 s to compile with clang, against 2.4 MB and 27 s at `-O3`; for comparison one SIMD
   feature set unit is about 0.35 MB. The C API and the bindings would get a
   `fnGenerateCode` entry.
3. **Node Editor**: "Copy as GLSL / HLSL / WGSL" next to "Copy Encoded Node Tree", and
   optionally a GPU preview that compiles the generated fragment shader (it would also be a
   permanent cross-check of CPU against GPU).
4. **Tests**: generate scalar C++ for the demo trees and every default-constructed node in
   CI, compile it, and compare against `GenSingle*` of the strict build; run glslang over the
   GLSL and HLSL output. The scratch versions of both exist in this session and are described
   above.
5. **FastSIMD C++ backend**: print the IR as `FS::` calls (`FS::FMulAdd`, `FS::Select`,
   `FS::Convert<int32_t>`, `float32v( c )`, ...) inside a `template<FeatureSet SIMD>`
   struct of `FS_FORCEINLINE` functions, plus a thin `DispatchClass<BakedTree, SIMD>` that
   forwards `Gen` to the entry function. Compiled through the existing custom-node mechanism
   this is the fully fused, virtual-call-free CPU evaluation of a tree. The printer is the
   scalar C++ one with different spellings.
6. **Tileable output**: a `GenTileable2D` style wrapper mapping 2D to 4D with cos/sin.
7. **Parameters as uniforms**: node members are read as plain scalars, so the tracer sees
   constants. Two cheap routes: regenerate on change (tracing a demo tree takes well under a
   millisecond, shader compilation dominates), or treat `Constant` nodes as exposed uniforms
   so artists can mark knobs in the editor. Making arbitrary members uniforms needs a small
   mechanical change in node code (a wrapper where members are broadcast), which is the first
   item on this list that would touch node sources.
8. **Rolled loops**: unrolling into calls is fine for fractals, but very large trees may want
   `for` loops. That needs a tracer-visible loop construct in node code (for example a
   `FS::Loop` helper or octave count as a template parameter) and is not needed for a first
   release.
9. **WGSL validation**: emitted by the same printer but not validated in this session (no
   naga or tint available); the known differences (unsigned shift amounts, `select` argument
   order, no overloading, literal rules) are handled, a validator run is still due.
