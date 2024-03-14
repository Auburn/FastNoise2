#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_WASM
#include "Internal/WASM.h"
#define FS_SIMD_CLASS FastSIMD::WASM
#include "Internal/SourceBuilder.inl"
#endif
