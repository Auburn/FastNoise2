#pragma once
#include <memory>
#include <string>

#include "Utility/Export.h"

namespace FastNoise
{
    struct NodeData;

    /** @brief Generate standalone source code from a node tree.
     *
     *  The node tree is traced: each node's SIMD implementation is run once on symbolic registers, recording the
     *  operations it performs with all node settings baked in as constants. The recorded program is then emitted
     *  as shader code, or as C++ for SIMD CPU generation without the per node virtual calls of a runtime tree.
     *
     *  Every node is supported automatically since the generated code is derived from the same implementation used
     *  for CPU generation. Integer hashing is reproduced exactly, so generated code produces the same noise as the
     *  CPU path up to floating point differences (GPU sqrt/pow/sin precision, FMA contraction).
     *
     *  @code
     *  std::string glsl = FastNoise::CodeGen::Generate( encodedNodeTree, FastNoise::CodeGen::Language::GLSL );
     *  // float fastnoise( vec3 pos, int seed )
     *  @endcode
     */
    namespace CodeGen
    {
        enum class Language
        {
            GLSL, ///< GLSL 3.30 / GLSL ES 3.00 (WebGL 2), entry point: float name( vecN pos, int seed )
            HLSL, ///< HLSL Shader Model 4+, entry point: float name( floatN pos, int seed )
            WGSL, ///< WebGPU Shading Language, entry point: fn name( pos: vecN<f32>, seed: i32 ) -> f32
            CPP,  ///< C++ using FastSIMD registers, compile in a FastSIMD dispatch library alongside FastNoise/Generators/Generator.inl
        };

        struct Options
        {
            int dimensions = 3;                       ///< Position dimensions of the entry point: 2, 3 or 4
            std::string functionName = "fastnoise";   ///< Name of the entry point, also used to prefix internal functions
            bool inlineAll = false;                   ///< Inline every node into the entry point instead of only nodes used once or small nodes
            bool fastMath = true;                     ///< Allow simplifications that only differ for -0, inf and NaN (x + 0 -> x, x * 0 -> 0, x < inf -> true)
        };

        /** @brief A traced node tree, can be emitted in multiple languages and evaluated as a reference. */
        class FASTNOISE_API Program
        {
        public:
            /** @brief Trace a node tree from an encoded string (as exported by the Node Editor).
             *  @param error  Optional, receives a description of why tracing failed.
             *  @return The traced program, or nullptr on failure.
             */
            static std::unique_ptr<Program> FromEncodedNodeTree( const char* encodedNodeTree, const Options& options = {}, std::string* error = nullptr );

            /** @brief Trace a node tree from node data. Unconnected node lookups are filled with Constant nodes, matching decoding. */
            static std::unique_ptr<Program> FromNodeData( const NodeData* nodeData, const Options& options = {}, std::string* error = nullptr );

            ~Program();
            Program( const Program& ) = delete;
            Program& operator=( const Program& ) = delete;

            /** @brief Emit source code for the program. */
            std::string Emit( Language language ) const;

            /** @brief Evaluate the program on the CPU, one sample at a time.
             *
             *  Reference implementation of the generated code. With Options::fastMath off it matches the values of
             *  FastNoise2's non relaxed (FASTNOISE2_STRICT_FP) SIMD output exactly, with it on they only differ where -0, inf or NaN occur.
             *  @param positions  Array of Options::dimensions positions.
             */
            float Evaluate( const float* positions, int seed ) const;

            int GetDimensions() const;
            int GetFunctionCount() const;
            size_t GetInstructionCount() const;

        private:
            struct Impl;
            explicit Program( Impl* impl );
            Impl* mImpl; // Not a std::unique_ptr, STL members of exported classes cause C4251 in DLL builds
        };

        /** @brief Shorthand to trace and emit an encoded node tree.
         *  @return Source code, or an empty string if the node tree could not be traced.
         */
        FASTNOISE_API std::string Generate( const char* encodedNodeTree, Language language, const Options& options = {} );
    }
}
