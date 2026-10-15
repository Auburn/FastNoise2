#pragma once
#include <string>

#include "Utility/Config.h"

namespace FastNoise
{
    struct NodeData;
}

namespace FastNoise::Codegen
{
    /** @brief Output language for generated node tree code. */
    enum class Language
    {
        GLSL,      ///< GLSL 3.30+ / ES 3.00+ (needs intBitsToFloat and roundEven)
        HLSL,      ///< HLSL shader model 4+
        WGSL,      ///< WebGPU shading language
        CppScalar, ///< Plain scalar C++ (reference implementation / tests)
    };

    /** @brief Options for Generate(). */
    struct Options
    {
        Language language = Language::GLSL;
        int dimensions = 3;                     ///< Number of position components: 2, 3 or 4
        const char* entryPoint = "FastNoise_Gen"; ///< Name of the generated entry function
        bool emitFma = false;                   ///< Emit fma()/mad() for multiply-adds instead of a * b + c
    };

    /** @brief Generate source code that evaluates a node tree.
     *
     *  The tree is evaluated symbolically using the same node implementations as the SIMD
     *  backends, so the output follows the CPU implementation exactly, including any node
     *  added later. Every node instance becomes one function per dimension count; the
     *  entry point takes `(int seed, float x, float y[, float z[, float w]])`.
     *
     *  @param encodedNodeTree  Encoded node tree string (see Metadata::SerialiseNodeData).
     *  @return Generated source, or an empty string if the tree could not be decoded.
     */
    FASTNOISE_API std::string Generate( const char* encodedNodeTree, const Options& options = {} );

    /** @brief Generate source code for a NodeData tree. */
    FASTNOISE_API std::string Generate( const NodeData& root, const Options& options = {} );
}
