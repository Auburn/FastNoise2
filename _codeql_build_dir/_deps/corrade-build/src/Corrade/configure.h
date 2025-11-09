#ifndef Corrade_configure_h
#define Corrade_configure_h
/*
    This file is part of Corrade.

    Copyright © 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016,
                2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#define CORRADE_MSVC_COMPATIBILITY
/* #undef CORRADE_MSVC2017_COMPATIBILITY */
/* #undef CORRADE_MSVC2015_COMPATIBILITY */

#define CORRADE_BUILD_DEPRECATED
#define CORRADE_BUILD_STATIC
/* #undef CORRADE_BUILD_STATIC_UNIQUE_GLOBALS */
#define CORRADE_BUILD_MULTITHREADED
#define CORRADE_BUILD_CPU_RUNTIME_DISPATCH

/* #undef CORRADE_TARGET_APPLE */
/* #undef CORRADE_TARGET_IOS */
/* #undef CORRADE_TARGET_IOS_SIMULATOR */
#define CORRADE_TARGET_UNIX
/* #undef CORRADE_TARGET_WINDOWS */
/* #undef CORRADE_TARGET_WINDOWS_RT */
/* #undef CORRADE_TARGET_EMSCRIPTEN */
/* #undef CORRADE_TARGET_ANDROID */

#define CORRADE_CPU_USE_IFUNC
/* #undef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT */
/* #undef CORRADE_TESTSUITE_TARGET_XCTEST */
/* #undef CORRADE_UTILITY_USE_ANSI_COLORS */

/* Cherry-picked from https://sourceforge.net/p/predef/wiki/Architectures/.
   Can't detect this stuff directly from CMake because of (for example) macOS
   and fat binaries. */

/* First two is GCC/Clang for 32/64 bit, second two is MSVC 32/64bit */
#if defined(__i386) || defined(__x86_64) || defined(_M_IX86) || defined(_M_X64)
#define CORRADE_TARGET_X86

/* First two is GCC/Clang for 32/64 bit, second two is MSVC 32/64bit. MSVC
   doesn't have AArch64 support in the compiler yet, though there are some
   signs of it in headers (http://stackoverflow.com/a/37251625/6108877). */
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
#define CORRADE_TARGET_ARM

/* First two is GCC/Clang, third is MSVC. Not sure about 64-bit MSVC. */
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(_M_PPC)
#define CORRADE_TARGET_POWERPC

/* WebAssembly (on Emscripten). Old pure asm.js toolchains did not define this,
   recent Emscripten does that even with `-s WASM=0`. */
#elif defined(__wasm__)
#define CORRADE_TARGET_WASM

/* No other platforms are currently tested for, but that's okay -- a runtime
   test for this is in Utility/Test/SystemTest.cpp */
#endif

/* Sanity checks. This might happen when using Emscripten-compiled code with
   native compilers, at which point we should just die. */
#if defined(CORRADE_TARGET_EMSCRIPTEN) && (defined(CORRADE_TARGET_X86) || defined(CORRADE_TARGET_ARM) || defined(CORRADE_TARGET_POWERPC))
#error CORRADE_TARGET_X86 / _ARM / _POWERPC defined on Emscripten
#endif

/* 64-bit WebAssembly macro was tested by passing -m64 to emcc */
#if !defined(__x86_64) && !defined(_M_X64) && !defined(__aarch64__) && !defined(_M_ARM64) && !defined(__powerpc64__) && !defined(__wasm64__)
#define CORRADE_TARGET_32BIT
#endif

/* C++ standard */
#ifdef _MSC_VER
#ifdef _MSVC_LANG
#define CORRADE_CXX_STANDARD _MSVC_LANG
#else
#define CORRADE_CXX_STANDARD 201103L
#endif
#else
#define CORRADE_CXX_STANDARD __cplusplus
#endif
#if CORRADE_CXX_STANDARD >= 201402
#define CORRADE_TARGET_CXX14
#endif
#if CORRADE_CXX_STANDARD >= 201703
#define CORRADE_TARGET_CXX17
#endif
#if CORRADE_CXX_STANDARD >= 202002
#define CORRADE_TARGET_CXX20
#endif

/* Standard library edition. Keep in sync with CorradeStl* singles. References:
   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65473
   https://github.com/gcc-mirror/gcc/commit/19665740d336d4ee7d0cf92b5b0643fa1d7da14a
   https://en.cppreference.com/w/cpp/header/ciso646 */
/* <ciso646> is removed in C++20 and replaced by <version>. As of August 2023
   MSVC 2022 warns that it's removed when using C++20, libc++ version 19 so far
   doesn't warn. Unfortunately libstdc++ as of version 15 warns already when
   using C++17, which is extremely annoying, because I can no longer expect
   that all C++17-enabled compilers have <ciso646> that works without warnings
   and all C++20-enabled compilers have <version> -- and *assuming* that all
   C++17-enabled compilers have <version> isn't going to work, because one can
   for example use a newer Clang with an older libstdc++ that doesn't have
   <version> yet.

   I also cannot make an exception for, say, _GLIBCXX_RELEASE >= 15 because I
   kinda need to include <ciso646> or <version> to get to the value of this
   macro, turning this into a stupid chicken-or-egg problem. Why the fuck did
   you have to *remove* the old header in the first place, just three years
   after it was marked deprecated, and, secondly, why THE FUCK it has to warn
   now, giving the codebase no headroom at all to paper over toolchain version
   differences?? Not every project is always on the latest GCC, for fucks sake.

   The only way to do this semi-sanely is thus first checking if __has_include
   is implemented by the compiler (because, guess what, it's *also* new in
   C++17, thus it may happen that <version> is there but __has_include not yet
   or vice versa, or any other combination, and __has_include is made in a way
   that its mere presence in a preprocessor expression is a syntax error if not
   implemented by the compiler), and then if it is, and <version> is there, and
   C++17 is used, include <version>, otherwise <ciso646>. Truly a design
   marvel, congratulations everyone involved, needing three paragraphs of
   explanatory comments for a basic STL version check that should have been
   just a single #include line if the involved parties just paused and thought
   for a moment. */
#if CORRADE_CXX_STANDARD >= 201703 && defined(__has_include)
    #if __has_include(<version>)
    #include <version>
    #else
    #include <ciso646>
    #endif
#else
#include <ciso646>
#endif
#ifdef _LIBCPP_VERSION
#define CORRADE_TARGET_LIBCXX
#elif defined(_CPPLIB_VER)
#define CORRADE_TARGET_DINKUMWARE
#elif defined(__GLIBCXX__)
#define CORRADE_TARGET_LIBSTDCXX
/* GCC's <ciso646> provides the __GLIBCXX__ macro only since 6.1, so on older
   versions we'll try to get it from bits/c++config.h */
#elif defined(__has_include)
    #if __has_include(<bits/c++config.h>)
        #include <bits/c++config.h>
        #ifdef __GLIBCXX__
        #define CORRADE_TARGET_LIBSTDCXX
        #endif
    #endif
/* GCC < 5.0 doesn't have __has_include, so on these versions we'll just assume
   it's libstdc++ as I don't think those versions are used with anything else
   nowadays anyway. Clang reports itself as GCC 4.4, so exclude that one. */
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5
#define CORRADE_TARGET_LIBSTDCXX
#else
/* Otherwise no idea. */
#endif

#ifdef __GNUC__
#define CORRADE_TARGET_GCC
#endif

#ifdef __clang__
#define CORRADE_TARGET_CLANG
#endif

#if defined(__clang__) && defined(_MSC_VER)
#define CORRADE_TARGET_CLANG_CL
#endif

#if defined(__clang__) && defined(__apple_build_version__)
/* The extra space is here to avoid issues with old & broken FindCorrade.cmake
   matching this as CORRADE_TARGET_APPLE. Should be removed in late 2025 when
   everyone has their find modules finally updated. */
#define  CORRADE_TARGET_APPLE_CLANG
#endif

#ifdef _MSC_VER
#define CORRADE_TARGET_MSVC
#endif

#ifdef __MINGW32__
#define CORRADE_TARGET_MINGW
#endif

/* First checking the GCC/Clang builtin, if available. As a fallback do an
   architecture-based check, which is mirrored from SDL_endian.h. Doing this
   *properly* would mean we can't decide this at compile time as some
   architectures allow switching endianness at runtime (and worse, have
   per-page endianness). So let's pretend we never saw this article:
    https://en.wikipedia.org/wiki/Endianness#Bi-endianness
   For extra safety this gets runtime-tested in TargetTest, so when porting
   to a new platform, make sure you run that test. */
#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define CORRADE_TARGET_BIG_ENDIAN
#elif __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error what kind of endianness is this?
#endif
#elif defined(__hppa__) || \
    defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
    (defined(__MIPS__) && defined(__MIPSEB__)) || \
    defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
    defined(__sparc__)
#define CORRADE_TARGET_BIG_ENDIAN
#endif
/* Can't really be marked as deprecated as it's only ever used in #ifdef
   statements. OTOH I don't want to remove this right away as it would cause
   temporary breakages until projects such as magnum-plugins update. */
#if defined(CORRADE_BUILD_DEPRECATED) && defined(CORRADE_TARGET_BIG_ENDIAN)
#define CORRADE_BIG_ENDIAN
#endif

/* Compile-time CPU feature detection */
#ifdef CORRADE_TARGET_X86

/* SSE on GCC: https://stackoverflow.com/a/28939692 */
/** @todo check also for Clang, maybe clang-cl defines these too? */
#ifdef CORRADE_TARGET_GCC
#ifdef __SSE2__
#define CORRADE_TARGET_SSE2
#endif
#ifdef __SSE3__
#define CORRADE_TARGET_SSE3
#endif
#ifdef __SSSE3__
#define CORRADE_TARGET_SSSE3
#endif
#ifdef __SSE4_1__
#define CORRADE_TARGET_SSE41
#endif
#ifdef __SSE4_2__
#define CORRADE_TARGET_SSE42
#endif

/* On MSVC: https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros */
#elif defined(CORRADE_TARGET_MSVC)
/* _M_IX86_FP is defined only on 32bit, 64bit has SSE2 always (so we need to
   detect 64bit instead: https://stackoverflow.com/a/18570487) */
#if (defined(_M_IX86_FP) && _M_IX86_FP == 2) || defined(_M_AMD64) || defined(_M_X64)
#define CORRADE_TARGET_SSE2
#endif
/* On MSVC there's no way to detect SSE3 and newer, these are only implied by
   AVX as far as I can tell */
#ifdef __AVX__
#define CORRADE_TARGET_SSE3
#define CORRADE_TARGET_SSSE3
#define CORRADE_TARGET_SSE41
#define CORRADE_TARGET_SSE42
#endif
#endif

/* Both GCC and MSVC have the same macros for AVX, AVX2 and AVX512F */
#if defined(CORRADE_TARGET_GCC) || defined(CORRADE_TARGET_MSVC)
#ifdef __AVX__
#define CORRADE_TARGET_AVX
#endif
#ifdef __AVX2__
#define CORRADE_TARGET_AVX2
#endif
#ifdef __AVX512F__
#define CORRADE_TARGET_AVX512F
#endif
#endif

/* POPCNT, LZCNT, BMI1 and BMI2 on GCC, queried wth
   `gcc -mpopcnt -dM -E - | grep POPCNT`, and equivalent for others. */
#ifdef CORRADE_TARGET_GCC
#ifdef __POPCNT__
#define CORRADE_TARGET_POPCNT
#endif
#ifdef __LZCNT__
#define CORRADE_TARGET_LZCNT
#endif
#ifdef __BMI__
#define CORRADE_TARGET_BMI1
#endif
#ifdef __BMI2__
#define CORRADE_TARGET_BMI2
#endif

/* There doesn't seem to be any equivalent on MSVC,
   https://github.com/kimwalisch/libpopcnt assumes POPCNT is on x86 MSVC
   always, and LZCNT has encoding compatible with BSR so if not available it'll
   not crash but produce wrong results, sometimes. Enabling them always feels a
   bit too much, so instead going with what clang-cl uses. There /arch:AVX
   matches -march=sandybridge:
    https://github.com/llvm/llvm-project/blob/6542cb55a3eb115b1c3592514590a19987ffc498/clang/lib/Driver/ToolChains/Arch/X86.cpp#L46-L58
   and `echo | clang -march=sandybridge -dM -E -` lists only POPCNT, while
   /arch:AVX2 matches -march=haswell, which lists also LZCNT, BMI and BMI2. */
#elif defined(CORRADE_TARGET_MSVC)
/* For extra robustness on clang-cl check the macros explicitly -- as with
   other AVX+ intrinsics, these are only included if the corresponding macro is
   defined as well. Failing to do so would mean the
   CORRADE_ENABLE_AVX_{POPCNT,LZCNT,BMI1} macros are defined always,
   incorrectly implying presence of these intrinsics. */
#ifdef __AVX__
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__POPCNT__)
#define CORRADE_TARGET_POPCNT
#endif
#endif
#ifdef __AVX2__
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__LZCNT__)
#define CORRADE_TARGET_LZCNT
#endif
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__BMI__)
#define CORRADE_TARGET_BMI1
#endif
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__BMI2__)
#define CORRADE_TARGET_BMI2
#endif
#endif
#endif

/* On GCC, F16C and FMA have its own define, on MSVC FMA is implied by
   /arch:AVX2  (https://docs.microsoft.com/en-us/cpp/build/reference/arch-x86),
   no mention of F16C but https://walbourn.github.io/directxmath-f16c-and-fma/
   says it's like that so I'll believe that. However, comments below
   https://stackoverflow.com/a/50829580 say there is a Via processor with AVX2
   but no FMA, so then the __AVX2__ check isn't really bulletproof. Use runtime
   detection where possible, please. */
#ifdef CORRADE_TARGET_GCC
#ifdef __F16C__
#define CORRADE_TARGET_AVX_F16C
#endif
#ifdef __FMA__
#define CORRADE_TARGET_AVX_FMA
#endif
#elif defined(CORRADE_TARGET_MSVC) && defined(__AVX2__)
/* On clang-cl /arch:AVX2 matches -march=haswell:
    https://github.com/llvm/llvm-project/blob/6542cb55a3eb115b1c3592514590a19987ffc498/clang/lib/Driver/ToolChains/Arch/X86.cpp#L46-L58
   And `echo | clang -march=haswell -dM -E -` lists both F16C and FMA. However,
   for robustness, check the macros explicitly -- as with other AVX+ intrinsics
   on clang-cl, these are only included if the corresponding macro is defined
   as well. Failing to do so would mean the CORRADE_ENABLE_AVX_{16C,FMA} macros
   are defined always, incorrectly implying presence of these intrinsics. */
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__F16C__)
#define CORRADE_TARGET_AVX_F16C
#endif
#if !defined(CORRADE_TARGET_CLANG_CL) || defined(__FMA__)
#define CORRADE_TARGET_AVX_FMA
#endif
#endif

/* https://stackoverflow.com/a/37056771, confirmed on Android NDK Clang that
   __ARM_NEON is indeed still set. For MSVC, according to
   https://docs.microsoft.com/en-us/cpp/intrinsics/arm-intrinsics I would
   assume that since they use a standard header, they also expose the standard
   macro name, even though not listed among their predefined macros? Needs
   testing, though. */
#elif defined(CORRADE_TARGET_ARM)
#ifdef __ARM_NEON
#define CORRADE_TARGET_NEON
/* NEON FMA is available only if __ARM_FEATURE_FMA is defined and some bits of
   __ARM_NEON_FP as well (ARM C Language Extensions 1.1, §6.5.5:
   https://developer.arm.com/documentation/ihi0053/b/). On AAArch64 NEON is
   implicitly supported and __ARM_NEON_FP might not be defined (Android Clang
   defines it but GCC 9 on Ubuntu ARM64 not), so check for __aarch64__ as
   well. */
#if defined(__ARM_FEATURE_FMA) && (__ARM_NEON_FP || defined(__aarch64__))
#define CORRADE_TARGET_NEON_FMA
#endif
/* There's no linkable documentation for anything and the PDF is stupid. But,
   given the FP16 instructions implemented in GCC and Clang are guarded by this
   macro, it should be alright:
   https://gcc.gnu.org/legacy-ml/gcc-patches/2016-06/msg00460.html */
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#define CORRADE_TARGET_NEON_FP16
#endif
#endif

/* Undocumented, checked via `echo | em++ -x c++ -dM -E - -msimd128`.
   Restricting to the finalized SIMD variant, which is since Clang 13:
    https://github.com/llvm/llvm-project/commit/502f54049d17f5a107f833596fb2c31297a99773
   Emscripten 2.0.13 sets Clang 13 as the minimum, however it doesn't imply
   that emsdk 2.0.13 actually contains the final Clang 13. That's only since
   2.0.18, thus to avoid nasty issues we have to check Emscripten version as
   well :(
    https://github.com/emscripten-core/emscripten/commit/deab7783df407b260f46352ffad2a77ca8fb0a4c */
#elif defined(CORRADE_TARGET_WASM)
/* The __EMSCRIPTEN_major__ etc macros used to be passed implicitly, version
   3.1.4 moved them to a version header and version 3.1.23 dropped the
   backwards compatibility. To work consistently on all versions, including the
   header only if the version macros aren't present.
   https://github.com/emscripten-core/emscripten/commit/f99af02045357d3d8b12e63793cef36dfde4530a
   https://github.com/emscripten-core/emscripten/commit/f76ddc702e4956aeedb658c49790cc352f892e4c */
/** @todo remove the include once we no longer need to check for 2.0.18 */
#if defined(CORRADE_TARGET_EMSCRIPTEN) && !defined(__EMSCRIPTEN_major__)
#include <emscripten/version.h>
#endif
#if defined(__wasm_simd128__) && __clang_major__ >= 13 && __EMSCRIPTEN_major__*10000 + __EMSCRIPTEN_minor__*100 + __EMSCRIPTEN_tiny__ >= 20018
#define CORRADE_TARGET_SIMD128
#endif
#endif

#if defined(CORRADE_TARGET_MSVC) || (defined(CORRADE_TARGET_ANDROID) && !__LP64__) || defined(CORRADE_TARGET_EMSCRIPTEN) || (defined(CORRADE_TARGET_APPLE) && !defined(CORRADE_TARGET_IOS) && defined(CORRADE_TARGET_ARM))
#define CORRADE_LONG_DOUBLE_SAME_AS_DOUBLE
#endif

#if defined(CORRADE_TARGET_LIBSTDCXX) && __GNUC__ < 5 && _GLIBCXX_RELEASE < 7
#define CORRADE_NO_STD_IS_TRIVIALLY_TRAITS
#endif

/* Kill switch for when absence of the /permissive- flag is detected and
   CORRADE_MSVC_COMPATIBILITY is not defined. According to
    https://developercommunity.visualstudio.com/t/pre-define-a-macro-when-compiling-under-permissive/1253982
   there's STILL no macro that advertising presence of the flag (independently
   verified with `cl /EP /Zc:preprocessor /PD empty.cpp 2>nul` on MSVC 2022).
   I suppose it's because it would break PCHs?

   However, because we need to remove various class members and do other nasty
   stuff to work without /permissive-, we need a way to control it from the
   preprocessor, which the CORRADE_MSVC_COMPATIBILITY macro is for. It's
   enabled implicitly but the user can explicitly disable it during the build
   of Corrade, which is then recorded into the generated configure.h file. The
   /permissive- flag is then expected to be passed.

   To avoid nasty issues with ABI mismatch, the following assert checks that
   the flag is passed if CORRADE_MSVC_COMPATIBILITY is not enabled. The hope is
   that CORRADE_MSVC_COMPATIBILITY gets eventually dropped altogether, with
   this assert being checked always. But as /permissive- caused ICEs in some
   MSVC 2017 versions, it's not feasible to enforce it yet.
*/
#if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG_CL) && !defined(CORRADE_MSVC_COMPATIBILITY)
/* Without /permissive- (or with /permissive- /Zc:ternary-, or with just
   /Zc:ternary), it'd be sizeof(const char*) instead. Yes, it's not
   bulletproof, but of all cases shown here it's the cheapest check:
   https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance?view=msvc-170#ambiguous-conditional-operator-arguments */
static_assert(sizeof(1 ? "" : "") == 1,
    "Corrade was built without CORRADE_MSVC_COMPATIBILITY, but /permissive- " "doesn't seem to be enabled. Either rebuild Corrade with the option "
    "enabled or ensure /permissive- is set for all files that include Corrade " "headers.");
#endif

/* Kill switch for when presence of a sanitizer is detected and
   CORRADE_CPU_USE_IFUNC is enabled. Unfortunately in our case the
   __attribute__((no_sanitize_address)) workaround as described on
   https://github.com/google/sanitizers/issues/342 doesn't work / can't be used
   because it would mean marking basically everything including the actual
   implementation that's being dispatched to. */
#ifdef CORRADE_CPU_USE_IFUNC
#ifdef __has_feature
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define _CORRADE_SANITIZER_IFUNC_DETECTED
#endif
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define _CORRADE_SANITIZER_IFUNC_DETECTED
#endif
#ifdef _CORRADE_SANITIZER_IFUNC_DETECTED
#error Corrade was built with CORRADE_CPU_USE_IFUNC, which is incompatible with sanitizers. Rebuild without this option or disable sanitizers.
#endif
#endif

#endif // kate: hl c++
