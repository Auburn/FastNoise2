#pragma once
#include <cassert>
#include <cmath>
#include <algorithm>
#include <atomic>

#ifdef FASTNOISE_METADATA
#include <tuple>
#endif

#include "FastNoise/Utility/Config.h"

#if !defined( FASTNOISE_METADATA ) && ( defined( __INTELLISENSE__ ) || defined( __CLION_IDE__ ) )
#define FASTNOISE_METADATA
#endif

namespace FastNoise
{
    /** @brief Represents a spatial dimension axis used for per-dimension configuration.
     *
     *  Used as a template parameter and array index for configuring node properties
     *  independently on each axis (e.g. domain offsets, axis scaling).
     */
    enum class Dim
    {
        X, Y, Z, W,
        Count
    };

    constexpr static const char* kDim_Strings[] =
    {
        "X", "Y", "Z", "W",
    };

    /** @brief Distance metrics used by Cellular noise and DistanceToPoint nodes.
     *
     *  Controls how the distance between points is calculated, which significantly
     *  affects the visual character of Cellular (Voronoi) noise patterns.
     */
    enum class DistanceFunction
    {
        Euclidean,        ///< Standard straight-line distance: sqrt(x^2 + y^2 + ...)
        EuclideanSquared, ///< Squared Euclidean distance (faster, skips sqrt). Default for Cellular noise.
        Manhattan,        ///< Sum of absolute axis differences: |x| + |y| + ... Produces diamond-shaped cells.
        Hybrid,           ///< EuclideanSquared + Manhattan combined. Produces organic, rounded cell shapes.
        MaxAxis,          ///< Maximum absolute difference along any single axis. Produces square-shaped cells.
        Minkowski,        ///< Generalised distance metric parameterised by P. P=1 is Manhattan, P=2 is Euclidean.
    };

    constexpr static const char* kDistanceFunction_Strings[] =
    {
        "Euclidean",
        "Euclidean Squared",
        "Manhattan",
        "Hybrid",
        "Max Axis",
        "Minkowski",
    };

    /** @brief Boolean enum used for metadata-driven node configuration (e.g. toggle parameters). */
    enum class Boolean
    {
        False,
        True
    };

    constexpr static const char* kBoolean_Strings[] =
    {
        "False",
        "True",
    };

    static constexpr float kInfinity =
#ifdef __GNUC__
        (float)(__builtin_inff ());
#else
        (float)(1e+300 * 1e+300);
#endif

    /** @brief Tracks the minimum and maximum values encountered during noise generation.
     *
     *  Returned by all batch generation methods (GenUniformGrid, GenPositionArray, GenTileable).
     *  Useful for normalising or remapping noise output to a desired range after generation.
     */
    struct OutputMinMax
    {
        float min =  kInfinity; ///< Smallest noise value generated in the batch.
        float max = -kInfinity; ///< Largest noise value generated in the batch.

        /** @brief Accumulate a single value into the tracked range. */
        OutputMinMax& operator <<( float v )
        {
            min = std::min( min, v );
            max = std::max( max, v );
            return *this;
        }

        /** @brief Merge another OutputMinMax into this one, expanding the tracked range. */
        OutputMinMax& operator <<( const OutputMinMax& v )
        {
            min = std::min( min, v.min );
            max = std::max( max, v.max );
            return *this;
        }
    };

    /** @brief Internal base class for node source connections.
     *
     *  Holds a reference to a source generator node and a pointer to its SIMD
     *  implementation. Used internally by the node graph to wire nodes together.
     *  @tparam T The generator type constraint for this source.
     */
    template<typename T>
    struct BaseSource
    {
        using Type = T;

        SmartNode<const T> base;          ///< Smart pointer to the source generator node.
        const void* simdGeneratorPtr = nullptr; ///< Internal SIMD dispatch pointer.

    protected:
        BaseSource() = default;
    };

    /** @brief A source input that must be connected to another generator node.
     *
     *  Used for required node inputs (e.g. Fractal::SetSource, DomainWarp::SetSource).
     *  The connected node must be set before generation; there is no fallback constant value.
     *  @tparam T The generator type constraint for this source.
     */
    template<typename T>
    struct GeneratorSourceT : BaseSource<T>
    { };

    /** @brief A source input that can be either a constant float value or a generator node.
     *
     *  Used for optional/hybrid node inputs where the user may provide a simple constant
     *  or wire in another generator for spatial variation (e.g. Fractal gain, warp amplitude).
     *  Defaults to the constant value if no generator node is connected.
     *  @tparam T The generator type constraint for this source.
     */
    template<typename T>
    struct HybridSourceT : BaseSource<T>
    {
        float constant; ///< The constant value used when no generator node is connected.

        constexpr HybridSourceT( float f = 0.0f ) : constant( f ) { }
    };

    namespace Internal
    {
        void BumpNodeRefences( const Generator*, bool );
    }

    /** @brief Abstract base class for all FastNoise2 noise generator nodes.
     *
     *  Generator is the root of the node class hierarchy. All noise types (Simplex, Perlin,
     *  Value, Cellular, etc.), fractals, operators, modifiers, and domain warps inherit from
     *  this class. Nodes are composed into a tree to build complex noise; the tree is then
     *  evaluated in a single SIMD-optimised pass.
     *
     *  **Creating nodes:** Use FastNoise::New<T>() to create a typed node, or
     *  FastNoise::NewFromEncodedNodeTree() to deserialise a node tree from an encoded string
     *  (e.g. one exported from the Node Editor tool).
     *
     *  **Thread safety:** All generation methods are const and fully thread-safe. The same
     *  node tree can be shared across multiple threads for parallel generation.
     *
     *  @note Generators are non-copyable and non-movable. Ownership is managed through SmartNode<T>.
     *
     *  @see SmartNode, Metadata, FastNoise::New
     */
    class FASTNOISE_API Generator
    {
    public:
        template<typename T>
        friend struct MetadataT;

        Generator() = default;
        Generator( const Generator& ) = delete;
        Generator( Generator&& ) = delete;

        virtual ~Generator() = default;

        /** @brief Returns the SIMD feature set (e.g. SSE2, AVX2, AVX512) this node was compiled for. */
        virtual FastSIMD::FeatureSet GetActiveFeatureSet() const = 0;

        /** @brief Returns the static Metadata object that holds this node's type, parameters, and descriptions. */
        virtual const Metadata& GetMetadata() const = 0;

        /** @brief Generate a 2D uniform grid of noise values.
         *
         *  Fills @p out with noise sampled on a regular 2D grid. Ideal for 
         *  generating noise for 2D textures, heightmaps, or image buffers that 
         *  require uniform spacing between sample positions.
         *
         *  Output values are written in row-major order with X as the inner loop:
         *  `out[y * xCount + x]`
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least `xCount * yCount` floats.
         *  @param      xOffset    Starting X position in world space.
         *  @param      yOffset    Starting Y position in world space.
         *  @param      xCount     Number of samples along the X axis.
         *  @param      yCount     Number of samples along the Y axis.
         *  @param      xStepSize  Distance between samples along X.
         *  @param      yStepSize  Distance between samples along Y.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenUniformGrid2D( float* out,
            float xOffset,   float yOffset,
              int xCount,      int yCount,
            float xStepSize, float yStepSize,
            int seed ) const = 0; 

        /** @brief Generate a 3D uniform grid of noise values.
         *
         *  Fills @p out with noise sampled on a regular 3D grid. Ideal for volumetric
         *  data such as voxel terrain, 3D textures, or density fields that 
         *  require uniform spacing between sample positions.
         *
         *  Try to avoid setting xCount = 1 if you want a slice of 3D noise, due to how
         *  the positions are generated a small xCount is bad for performance.
         *  Use y/zCount = 1 instead.
         *
         *  Output values are written with X as the innermost loop, then Y, then Z:
         *  `out[(z * yCount + y) * xCount + x]`
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least `xCount * yCount * zCount` floats.
         *  @param      xOffset    Starting X position in world space.
         *  @param      yOffset    Starting Y position in world space.
         *  @param      zOffset    Starting Z position in world space.
         *  @param      xCount     Number of samples along the X axis.
         *  @param      yCount     Number of samples along the Y axis.
         *  @param      zCount     Number of samples along the Z axis.
         *  @param      xStepSize  Distance between samples along X.
         *  @param      yStepSize  Distance between samples along Y.
         *  @param      zStepSize  Distance between samples along Z.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenUniformGrid3D( float* out,
            float xOffset,   float yOffset,   float zOffset,
              int xCount,      int yCount,      int zCount,
            float xStepSize, float yStepSize, float zStepSize,
            int seed ) const = 0;

        /** @brief Generate a 4D uniform grid of noise values.
         *
         *  Fills @p out with noise sampled on a regular 4D grid. The fourth dimension can be
         *  used for animating 3D noise over time or other parametric effects.
         *
         *  Try to avoid setting xCount = 1 if you want a slice of 4D noise, due to how
         *  the positions are generated a small xCount is bad for performance.
         *  Use y/z/wCount = 1 instead.
         *
         *  Output values are written with X as the innermost loop, then Y, Z, W:
         *  `out[((w * zCount + z) * yCount + y) * xCount + x]`
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least `xCount * yCount * zCount * wCount` floats.
         *  @param      xOffset    Starting X position in world space.
         *  @param      yOffset    Starting Y position in world space.
         *  @param      zOffset    Starting Z position in world space.
         *  @param      wOffset    Starting W position in world space.
         *  @param      xCount     Number of samples along the X axis.
         *  @param      yCount     Number of samples along the Y axis.
         *  @param      zCount     Number of samples along the Z axis.
         *  @param      wCount     Number of samples along the W axis.
         *  @param      xStepSize  Distance between samples along X.
         *  @param      yStepSize  Distance between samples along Y.
         *  @param      zStepSize  Distance between samples along Z.
         *  @param      wStepSize  Distance between samples along W.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenUniformGrid4D( float* out,
            float xOffset,   float yOffset,   float zOffset,   float wOffset,
              int xCount,      int yCount,      int zCount,      int wCount,
            float xStepSize, float yStepSize, float zStepSize, float wStepSize,
            int seed ) const = 0;

        /** @brief Generate seamlessly tileable 2D noise.
         *
         *  Produces a 2D noise image that tiles perfectly when repeated in both X and Y.
         *  Internally this works by mapping the 2D grid onto a 4D hypertorus, so the
         *  underlying noise types will be using 4D generation.
         *
         *  Output is written in row-major order: `out[y * xSize + x]`
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least `xSize * ySize` floats.
         *  @param      xSize      Number of samples (and tile width) along the X axis.
         *  @param      ySize      Number of samples (and tile height) along the Y axis.
         *  @param      xStepSize  Distance between samples along X.
         *  @param      yStepSize  Distance between samples along Y.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenTileable2D( float* out,
            int xSize, int ySize,
            float xStepSize, float yStepSize,
            int seed ) const = 0; 

        /** @brief Generate noise at arbitrary 2D positions.
         *
         *  Evaluates noise at a set of caller-supplied (x, y) positions. Use this when
         *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
         *  positions, or any non-grid layout).
         *
         *  This is faster than using GenUniformGrid2D since the uniform grid positions 
         *  don't need to be generated. So for maximum generation performance across many
         *  generation calls: pre generate your own uniform grid positions once and reuse
         *  them for all generation calls with x/yOffset as needed.
         *
         *  Each position is calculated as `Vector2(xPosArray[i] + xOffset, yPosArray[i] + yOffset)`.
         *  The offset parameters allow you to shift all positions without modifying the arrays.
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least @p count floats.
         *  @param      count      Number of positions to generate.
         *  @param      xPosArray  Array of X coordinates in world space (length >= @p count).
         *  @param      yPosArray  Array of Y coordinates in world space (length >= @p count).
         *  @param      xOffset    Constant offset added to all X positions.
         *  @param      yOffset    Constant offset added to all Y positions.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenPositionArray2D( float* out, int count,
            const float* xPosArray, const float* yPosArray,
            float xOffset, float yOffset, int seed ) const = 0;

        /** @brief Generate noise at arbitrary 3D positions.
         *
         *  Evaluates noise at a set of caller-supplied (x, y, z) positions. Use this when
         *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
         *  positions, or any non-grid layout).
         *
         *  This is faster than using GenUniformGrid3D since the uniform grid positions 
         *  don't need to be generated. So for maximum generation performance across many
         *  generation calls: pre generate your own uniform grid positions once and reuse
         *  them for all generation calls with x/y/zOffset as needed.
         *
         *  Each position is calculated as `Vector3(xPosArray[i] + xOffset, yPosArray[i] + yOffset, zPosArray[i] + zOffset)`.
         *  The offset parameters allow you to shift all positions without modifying the arrays.
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least @p count floats.
         *  @param      count      Number of positions to generate.
         *  @param      xPosArray  Array of X coordinates in world space (length >= @p count).
         *  @param      yPosArray  Array of Y coordinates in world space (length >= @p count).
         *  @param      zPosArray  Array of Z coordinates in world space (length >= @p count).
         *  @param      xOffset    Constant offset added to all X positions.
         *  @param      yOffset    Constant offset added to all Y positions.
         *  @param      zOffset    Constant offset added to all Z positions.
         *  @param      seed       Seed value for the noise. Different seeds produce different patterns.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenPositionArray3D( float* out, int count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, 
            float xOffset, float yOffset, float zOffset, int seed ) const = 0;

        /** @brief Generate noise at arbitrary 4D positions.
         *  Evaluates noise at a set of caller-supplied (x, y, z, w) positions. Use this when
         *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
         *  positions, or any non-grid layout).
         *
         *  This is faster than using GenUniformGrid4D since the uniform grid positions 
         *  don't need to be generated. So for maximum generation performance across many
         *  generation calls: pre generate your own uniform grid positions once and reuse
         *  them for all generation calls with x/y/z/wOffset as needed.
         *
         *  Each position is calculated as `Vector4(xPosArray[i] + xOffset, yPosArray[i] + yOffset, zPosArray[i] + zOffset, wPosArray[i] + wOffset)`.
         *  The offset parameters allow you to shift all positions without modifying the arrays.
         *
         *  @param[out] out        Pre-allocated output array. Must hold at least @p count floats.
         *  @param      count      Number of positions to generate.
         *  @param      xPosArray  Array of X coordinates in world space (length >= @p count).
         *  @param      yPosArray  Array of Y coordinates in world space (length >= @p count).
         *  @param      zPosArray  Array of Z coordinates in world space (length >= @p count).
         *  @param      wPosArray  Array of W coordinates in world space (length >= @p count).
         *  @param      xOffset    Constant offset added to all X positions.
         *  @param      yOffset    Constant offset added to all Y positions.
         *  @param      zOffset    Constant offset added to all Z positions.
         *  @param      wOffset    Constant offset added to all W positions.
         *  @param      seed       Seed value for the noise.
         *  @return The min and max noise values written to @p out.
         */
        virtual OutputMinMax GenPositionArray4D( float* out, int count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, 
            float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const = 0;

        /** @brief Generate a single 2D noise value at a specific position. VERY SLOW!!!
         *
         *  Avoid using this unless you only need a single sample, this is significantly slower
         *  per-sample than the batch methods (GenUniformGrid, GenPositionArray) because SIMD
         *  lanes are underutilised. Prefer batch methods for bulk generation.
         *
         *  @param x    X position in world space.
         *  @param y    Y position in world space.
         *  @param seed Seed value for the noise. Different seeds produce different patterns.
         *  @return The noise value at the given position.
         */
        virtual float GenSingle2D( float x, float y, int seed ) const = 0;

        /** @brief Generate a single 3D noise value at a specific position. VERY SLOW!!!
         *
         *  Avoid using this unless you only need a single sample, this is significantly slower
         *  per-sample than the batch methods (GenUniformGrid, GenPositionArray) because SIMD
         *  lanes are underutilised. Prefer batch methods for bulk generation.
         *
         *  @param x    X position in world space.
         *  @param y    Y position in world space.
         *  @param z    Z position in world space.
         *  @param seed Seed value for the noise. Different seeds produce different patterns.
         *  @return The noise value at the given position.
         */
        virtual float GenSingle3D( float x, float y, float z, int seed ) const = 0;

        /** @brief Generate a single 4D noise value at a specific position. VERY SLOW!!!
         *
         *  Avoid using this unless you only need a single sample, this is significantly slower
         *  per-sample than the batch methods (GenUniformGrid, GenPositionArray) because SIMD
         *  lanes are underutilised. Prefer batch methods for bulk generation.
         *
         *  @param x    X position in world space.
         *  @param y    Y position in world space.
         *  @param z    Z position in world space.
         *  @param w    W position in world space.
         *  @param seed Seed value for the noise. Different seeds produce different patterns.
         *  @return The noise value at the given position.
         */
        virtual float GenSingle4D( float x, float y, float z, float w, int seed ) const = 0;

    protected:
        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, SmartNodeArg<T> gen )
        {
            static_assert( std::is_base_of<Generator, T>::value, "T must be child of FastNoise::Generator class" );

            assert( !gen.get() || GetActiveFeatureSet() == gen->GetActiveFeatureSet() ); // Ensure that all SIMD levels match

            SetSourceSIMDPtr( static_cast<const Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
            memberVariable.base = gen;
        }

    private:
        virtual void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) = 0;
        virtual int32_t ReferencesFetchAdd( int32_t add = 0 ) const noexcept = 0;

        template<typename>
        friend class SmartNode;
        friend void Internal::BumpNodeRefences( const Generator*, bool );
    };

    using GeneratorSource = GeneratorSourceT<Generator>; ///< Shorthand for a required generator source input.
    using HybridSource = HybridSourceT<Generator>;     ///< Shorthand for a hybrid (constant or generator) source input.

    /** @brief Holds a value for each spatial dimension (X, Y, Z, W).
     *
     *  Used by nodes that have independently configurable per-axis parameters,
     *  such as DomainOffset or DomainAxisScale. Each element in the array
     *  corresponds to one dimension as indexed by Dim.
     *
     *  @tparam T The value type stored per dimension (typically float or HybridSource).
     */
    template<typename T>
    struct PerDimensionVariable
    {
        using Type = T;

        T varArray[(int)Dim::Count]; ///< One value per dimension, indexed by Dim.

        template<typename U = T>
#if __cplusplus >= 201402L
        constexpr
#endif
        PerDimensionVariable( U value = 0 )
        {
            for( T& element : varArray )
            {
                element = value;
            }
        }

        T& operator[]( size_t i )
        {
            return varArray[i];
        }

        const T& operator[]( size_t i ) const
        {
            return varArray[i];
        }
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Generator> : Metadata
    {
    protected:
        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( NameDesc nameDesc, T defaultV, U&& func, T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultV;
            member.valueUiDragSpeed = uiDragSpeed;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                {
                    func( gRealType, v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename V, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( NameDesc nameDesc, T defaultV, V ( U::*func )( T ), T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultV;
            member.valueUiDragSpeed = uiDragSpeed;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<std::is_enum_v<T>>, typename... ENUM_NAMES>
        void AddVariableEnum( NameDesc nameDesc, T defaultV, void(U::* func)(T), ENUM_NAMES... enumNames )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            ( member.enumNames.push_back( enumNames ), ... );

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, size_t ENUM_NAMES, typename = std::enable_if_t<std::is_enum_v<T>>>
        void AddVariableEnum( NameDesc nameDesc, T defaultV, void(U::* func)(T), const char* const (&enumNames)[ENUM_NAMES] )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            for( const char* enumName : enumNames )
            {
                member.enumNames.push_back( enumName );
            }

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddPerDimensionVariable( NameDesc nameDesc, T defaultV, U&& func, T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberVariable member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.valueDefault = defaultV;
                member.valueUiDragSpeed = uiDragSpeed;
                member.valueMin = minV;
                member.valueMax = maxV;

                member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, MemberVariable::ValueUnion v )
                {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberVariables.push_back( member );
            }
        }

        template<typename T, typename U>
        void AddGeneratorSource( NameDesc nameDesc, void(U::* func)(SmartNodeArg<T>) )
        {
            MemberNodeLookup member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;

            member.setFunc = [func]( Generator* g, SmartNodeArg<> s )
            {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast ); 
                        (gRealType->*func)( source );
                        return true;
                    }
                }
                return false;
            };

            memberNodeLookups.push_back( member );
        }

        template<typename U>
        void AddPerDimensionGeneratorSource( NameDesc nameDesc, U&& func )
        {
            using GeneratorSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename GeneratorSourceT::Type;

            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberNodeLookup member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, SmartNodeArg<> s )
                {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast ); 
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                memberNodeLookups.push_back( member );
            }
        }


        template<typename T, typename U>
        void AddHybridSource( NameDesc nameDesc, float defaultValue, void ( U::*funcNode )( SmartNodeArg<T> ), void ( U::*funcValue )( float ), float uiDragSpeed = Metadata::kDefaultUiDragSpeedFloat )
        {
            MemberHybrid member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultValue;
            member.valueUiDragSpeed = uiDragSpeed;

            member.setNodeFunc = [funcNode]( Generator* g, SmartNodeArg<> s )
            {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast ); 
                        (gRealType->*funcNode)( source );
                        return true;
                    }
                }
                return false;
            };

            member.setValueFunc = [funcValue]( Generator* g, float v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*funcValue)( v );
                    return true;
                }                
                return false;
            };

            memberHybrids.push_back( member );
        }

        template<typename U>
        void AddPerDimensionHybridSource( NameDesc nameDesc, float defaultV, U&& func, float uiDragSpeed = Metadata::kDefaultUiDragSpeedFloat )
        {
            using HybridSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename HybridSourceT::Type;

            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberHybrid member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.valueDefault = defaultV;
                member.valueUiDragSpeed = uiDragSpeed;
                member.dimensionIdx = idx;

                member.setNodeFunc = [func, idx]( Generator* g, SmartNodeArg<> s )
                {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast ); 
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                member.setValueFunc = [func, idx]( Generator* g, float v )
                {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberHybrids.push_back( member );
            }
        }

    private:
        template<typename F, typename Ret, typename... Args>
        static std::tuple<Args...> GetArg_Helper( Ret( F::* )(Args...) const );

        template<typename F, size_t I>
        using GetArg = std::tuple_element_t<I, decltype(GetArg_Helper( &F::operator() ))>;
    };
#endif
}
