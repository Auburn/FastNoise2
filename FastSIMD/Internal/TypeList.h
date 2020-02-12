#pragma once

#include "FastSIMD.h"
#include <type_traits>

template<FastSIMD::ELevel L>
struct DummySIMDClass
{
    static const FastSIMD::ELevel SIMD_Level = L;
};

#if FASTSIMD_COMPILE_SCALAR
#include "Scalar.h"
#else
namespace FastSIMD
{
    typedef DummySIMDClass<Level_Scalar> Scalar
}
#endif

#if FASTSIMD_COMPILE_SSE | FASTSIMD_COMPILE_SSE2 | FASTSIMD_COMPILE_SSE3 | FASTSIMD_COMPILE_SSSE3 | FASTSIMD_COMPILE_SSE41 | FASTSIMD_COMPILE_SSE42
#include "SSE.h"
#endif
namespace FastSIMD
{
#if !(FASTSIMD_COMPILE_SSE)
    typedef DummySIMDClass<Level_SSE> SSE;
#endif
#if !(FASTSIMD_COMPILE_SSE2)
    typedef DummySIMDClass<Level_SSE2> SSE2;
#endif
#if !(FASTSIMD_COMPILE_SSE3)
    typedef DummySIMDClass<Level_SSE3> SSE3;
#endif
#if !(FASTSIMD_COMPILE_SSSE3)
    typedef DummySIMDClass<Level_SSSE3> SSSE3;
#endif
#if !(FASTSIMD_COMPILE_SSE41)
    typedef DummySIMDClass<Level_SSE41> SSE41;
#endif
#if !(FASTSIMD_COMPILE_SSE42)
    typedef DummySIMDClass<Level_SSE42> SSE42;
#endif
}

#if FASTSIMD_COMPILE_AVX | FASTSIMD_COMPILE_AVX2
#include "AVX.h"
#endif
namespace FastSIMD
{
#if !(FASTSIMD_COMPILE_AVX)
    typedef DummySIMDClass<Level_AVX> AVX;
#endif
#if !(FASTSIMD_COMPILE_AVX2)
    typedef DummySIMDClass<Level_AVX2> AVX2;
#endif
}

#if FASTSIMD_COMPILE_AVX512
#include "AVX512.h"
#else
namespace FastSIMD
{
    typedef DummySIMDClass<Level_AVX512> AVX512;
}
#endif

#if FASTSIMD_COMPILE_NEON
#include "NEON.h"
#else
namespace FastSIMD
{
    typedef DummySIMDClass<Level_NEON> NEON;
}
#endif

namespace FastSIMD
{
    template<typename... T>
    struct SIMDClassContainer
    {
        using MinimumCompiled = void;

        template<ELevel L>
        using GetByLevel = void;

        template<typename CLASS>
        using GetNextCompiledAfter = void;
    };

    template<typename HEAD, typename... TAIL>
    struct SIMDClassContainer<HEAD, TAIL...>
    {
        using MinimumCompiled = typename std::conditional<(HEAD::SIMD_Level & COMPILED_SIMD_LEVELS) != 0, HEAD, typename SIMDClassContainer<TAIL...>::MinimumCompiled>::type;

        template<ELevel L>
        using GetByLevel = typename std::conditional< L == HEAD::SIMD_Level, HEAD, typename SIMDClassContainer<TAIL...>::template GetByLevel<L> >::type;

        template<typename CLASS>
        using GetNextCompiledAfter = typename std::conditional< std::is_same<CLASS, HEAD>::value, typename SIMDClassContainer<TAIL...>::MinimumCompiled, typename SIMDClassContainer<TAIL...>::template GetNextCompiledAfter<CLASS> >::type;
    };

    typedef SIMDClassContainer<
        Scalar,
        SSE,
        SSE2,
        SSE3,
        SSSE3,
        SSE41,
        SSE42,
        AVX,
        AVX2,
        AVX512,
        NEON
    >
        SIMDClassList;

    static_assert(SIMDClassList::MinimumCompiled::SIMD_Level == FASTSIMD_FALLBACK_SIMD_LEVEL, "FASTSIMD_FALLBACK_SIMD_LEVEL is not the lowest compiled SIMD level");
}