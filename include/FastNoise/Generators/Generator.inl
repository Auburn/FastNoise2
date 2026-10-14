#include <cassert>
#include <cstring>
#include <utility>
#include <vector>

#include "Generator.h"

#pragma warning( disable:4250 )

using namespace FastNoise;

static constexpr size_t kRegisterSize = std::max<size_t>( 4, FS::NativeRegisterCount<float>() * 2 );
using float32v = FS::Register<float, kRegisterSize>;
using int32v = FS::Register<std::int32_t, kRegisterSize>;
using mask32v = typename float32v::MaskType;

// Attributes for the static Gen functions: disable tail calls, clang restores the callee saved xmm8-15 before a FS_VECTORCALL
// tail call even when they hold 4D arguments. The AVX sources keep frame pointers (src/CMakeLists.txt), so clang aligns
// AVX spills by realigning the stack only in functions that spill
#if defined( __clang__ )
#define FASTNOISE_GEN_FN_ATTRIBUTES __attribute__(( disable_tail_calls ))
#else
#define FASTNOISE_GEN_FN_ATTRIBUTES
#endif

// Block evaluation paths are kept out of line, so a generator's per vector path stays as small as without them
#if defined( _MSC_VER ) && !defined( __clang__ )
#define FASTNOISE_NOINLINE __declspec( noinline )
#else
#define FASTNOISE_NOINLINE __attribute__(( noinline ))
#endif

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Generator, SIMD> : public virtual Generator
{
public:
    // Gen() is dispatched through function pointers to static functions instead of virtual member functions:
    // clang-cl returns float32v through a hidden pointer (always from member functions, and from any function since float32v
    // has user-provided constructors), a plain aggregate returned from a static function stays in registers
#if defined( _MSC_VER ) && !defined( __clang__ )
    // MSVC copies FastSIMD registers returned in zmm registers through the stack in 256 bit halves, use the native types
    struct GenResult
    {
        typename float32v::DoubledType::NativeType v0, v1;
    };

    static FS_FORCEINLINE GenResult ToGenResult( const float32v& v )
    {
        return { v.v0.GetNative(), v.v1.GetNative() };
    }
#else
    struct GenResult
    {
        typename float32v::DoubledType v0, v1;
    };

    static FS_FORCEINLINE GenResult ToGenResult( const float32v& v )
    {
        return { v.v0, v.v1 };
    }
#endif

    using GenFn2D = GenResult( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v );
    using GenFn3D = GenResult( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v, float32v );
    using GenFn4D = GenResult( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v, float32v, float32v );

    static FS_FORCEINLINE float32v CallGen( const DispatchClass* gen, int32v seed, float32v x, float32v y )
    {
        assert( gen->mGenFn2D );
        GenResult result = gen->mGenFn2D( gen, seed, x, y );
        return float32v( result.v0, result.v1 );
    }

    static FS_FORCEINLINE float32v CallGen( const DispatchClass* gen, int32v seed, float32v x, float32v y, float32v z )
    {
        assert( gen->mGenFn3D );
        GenResult result = gen->mGenFn3D( gen, seed, x, y, z );
        return float32v( result.v0, result.v1 );
    }

    static FS_FORCEINLINE float32v CallGen( const DispatchClass* gen, int32v seed, float32v x, float32v y, float32v z, float32v w )
    {
        assert( gen->mGenFn4D );
        GenResult result = gen->mGenFn4D( gen, seed, x, y, z, w );
        return float32v( result.v0, result.v1 );
    }

    // Evaluates count values from positions stored per vector as { x[N], y[N], ... } (N = float32v::ElementCount)
    using GenBlockFn = void( FS_VECTORCALL* )( const DispatchClass*, int32v, const float*, float*, intptr_t );

    template<size_t DIMS>
    static FS_FORCEINLINE void CallGenBlock( const DispatchClass* gen, int32v seed, const float* pos, float* out, intptr_t count )
    {
        assert( gen->mGenBlockFn[DIMS - 2] );
        gen->mGenBlockFn[DIMS - 2]( gen, seed, pos, out, count );
    }

// Defines the static Gen and GenBlock functions of a generator and points the function pointers at them,
// GENFUNC( seed, x, y[, z[, w]] ) is the (force inlined) member function implementing the generator
#define FASTNOISE_IMPL_GEN_FN( GENFUNC )\
    using GenResultT = typename DispatchClass<Generator, SIMD>::GenResult;\
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL GenFn( const DispatchClass<Generator, SIMD>* gen, int32v seed, float32v x, float32v y )\
    {\
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const DispatchClass*>( gen )->GENFUNC( seed, x, y ) );\
    }\
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL GenFn( const DispatchClass<Generator, SIMD>* gen, int32v seed, float32v x, float32v y, float32v z )\
    {\
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const DispatchClass*>( gen )->GENFUNC( seed, x, y, z ) );\
    }\
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL GenFn( const DispatchClass<Generator, SIMD>* gen, int32v seed, float32v x, float32v y, float32v z, float32v w )\
    {\
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const DispatchClass*>( gen )->GENFUNC( seed, x, y, z, w ) );\
    }\
    template<size_t... DIM>\
    FASTNOISE_GEN_FN_ATTRIBUTES static void FS_VECTORCALL GenBlockFn( const DispatchClass<Generator, SIMD>* gen, int32v seed, const float* pos, float* out, intptr_t count )\
    {\
        if constexpr( DispatchClass::kHasGenBlock )\
        {\
            if( static_cast<const DispatchClass*>( gen )->template GenBlock<sizeof...( DIM )>( seed, pos, out, count ) )\
            {\
                return;\
            }\
        }\
        for( intptr_t i = 0; i < count; i += int32v::ElementCount )\
        {\
            GenResultT result = GenFn( gen, seed, FS::Load<float32v>( pos + i * sizeof...( DIM ) + DIM * int32v::ElementCount )... );\
            FS::Store( out + i, float32v( result.v0, result.v1 ) );\
        }\
    }\
public:\
    DispatchClass()\
    {\
        this->mGenFn2D = &GenFn;\
        this->mGenFn3D = &GenFn;\
        this->mGenFn4D = &GenFn;\
        this->mGenBlockFn[0] = &GenBlockFn<0, 1>;\
        this->mGenBlockFn[1] = &GenBlockFn<0, 1, 2>;\
        this->mGenBlockFn[2] = &GenBlockFn<0, 1, 2, 3>;\
        this->mHasGenBlock = DispatchClass::kHasGenBlock;\
    }\
private:

#define FASTNOISE_IMPL_GEN_T FASTNOISE_IMPL_GEN_FN( GenT )

    // Generators that evaluate a whole position block themselves (e.g. calling their sources' block functions once per block
    // instead of once per vector) set this and define template<size_t DIMS> bool GenBlock( seed, pos, out, count ), returning
    // false to have the block evaluated per vector instead. Its block buffers are kept in functions that aren't inlined
    // (FASTNOISE_NOINLINE), so they don't add stack probes and spills to the per vector loop
    static constexpr bool kHasGenBlock = false;

    // Number of values in a position block, blocks hold up to this many values (a multiple of the vector size)
    static constexpr intptr_t kGenBlockSize = int32v::ElementCount * 16;

    // Each nested GenBlock keeps block buffers on the stack, past this depth sources are evaluated per vector
    static constexpr int kMaxGenBlockDepth = 8;

    static FS_FORCEINLINE int& GenBlockDepth()
    {
        thread_local int depth = 0;
        return depth;
    }

    // Evaluates a source for a block from a GenBlock, per vector past kMaxGenBlockDepth nested block calls
    template<size_t DIMS>
    static FS_FORCEINLINE void CallSourceBlock( const DispatchClass* gen, int32v seed, const float* pos, float* out, intptr_t count )
    {
        int& depth = GenBlockDepth();

        if( depth < kMaxGenBlockDepth )
        {
            depth++;
            CallGenBlock<DIMS>( gen, seed, pos, out, count );
            depth--;
            return;
        }

        ForEachBlockVector<DIMS>( pos, count, [&]( intptr_t i, auto... p ) { FS::Store( out + i, CallGen( gen, seed, p... ) ); } );
    }

    // True if the source evaluates blocks with its own GenBlock (generators with sources). Evaluating a generator without one
    // per vector with the positions in registers is faster than storing computed positions for a block and reloading them
    template<typename T>
    static FS_FORCEINLINE bool SourceHasGenBlock( const FastNoise::GeneratorSourceT<T>& memberVariable )
    {
        assert( memberVariable.simdGeneratorPtr );
        return reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr )->mHasGenBlock;
    }

    template<size_t DIMS, typename T>
    static FS_FORCEINLINE void GetSourceBlock( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, const float* pos, float* out, intptr_t count )
    {
        assert( memberVariable.simdGeneratorPtr );
        CallSourceBlock<DIMS>( reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr ), seed, pos, out, count );
    }

    // Calls f( i, x, y[, z[, w]] ) for the vector of positions at each output index i of a position block
    template<size_t DIMS, typename F>
    static FS_FORCEINLINE void ForEachBlockVector( const float* pos, intptr_t count, F&& f )
    {
        ForEachBlockVector( std::make_index_sequence<DIMS>(), pos, count, f );
    }

    template<size_t... I, typename F>
    static FS_FORCEINLINE void ForEachBlockVector( std::index_sequence<I...>, const float* pos, intptr_t count, F& f )
    {
        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            f( i, FS::Load<float32v>( pos + i * sizeof...( I ) + I * int32v::ElementCount )... );
        }
    }

    // Value of a source for the vector at output index i of a position block, evaluated on its own
    template<size_t DIMS, typename S>
    static FS_FORCEINLINE float32v GetSourceValueAt( const S& memberVariable, int32v seed, const float* pos, intptr_t i )
    {
        return GetSourceValueAt( memberVariable, seed, pos + i * DIMS, std::make_index_sequence<DIMS>() );
    }

    template<typename S, size_t... I>
    static FS_FORCEINLINE float32v GetSourceValueAt( const S& memberVariable, int32v seed, const float* vectorPos, std::index_sequence<I...> )
    {
        return GetSourceValue( memberVariable, seed, FS::Load<float32v>( vectorPos + I * int32v::ElementCount )... );
    }

    // True if any of the generator sources evaluates blocks with its own GenBlock, see SourceHasGenBlock
    template<typename... S>
    static FS_FORCEINLINE bool AnySourceHasGenBlock( const S&... memberVariables )
    {
        return ( IsGenBlockSource( memberVariables ) || ... );
    }

    template<typename T>
    static FS_FORCEINLINE bool IsGenBlockSource( const FastNoise::GeneratorSourceT<T>& memberVariable )
    {
        return SourceHasGenBlock( memberVariable );
    }

    template<typename T>
    static FS_FORCEINLINE bool IsGenBlockSource( const FastNoise::HybridSourceT<T>& memberVariable )
    {
        return memberVariable.simdGeneratorPtr && reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr )->mHasGenBlock;
    }

    // Stores the position vectors for output index i of a block at dst = blockPos + i * DIMS
    template<typename... P>
    static FS_FORCEINLINE void StoreBlockVector( float* dst, P... pos )
    {
        ( ( FS::Store( dst, pos ), dst += int32v::ElementCount ), ... );
    }

    // Evaluates the source at the positions transform( dst, i, pos... ) stores for the vector at each output index i (with StoreBlockVector)
    template<size_t DIMS, size_t SOURCE_DIMS, typename T, typename F>
    static FASTNOISE_NOINLINE void GetSourceBlockAt( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, const float* pos, float* out, intptr_t count, F&& transform )
    {
        alignas( 64 ) float sourcePos[kGenBlockSize * SOURCE_DIMS];

        ForEachBlockVector<DIMS>( pos, count, [&]( intptr_t i, auto... p ) { transform( sourcePos + i * SOURCE_DIMS, i, p... ); } );

        GetSourceBlock<SOURCE_DIMS>( memberVariable, seed, sourcePos, out, count );
    }

    // Source values of a block: generator output in a buffer, or the constant of a hybrid source. The constant is read through
    // a pointer in every iteration, so loops can't hoist maths on it (e.g. x / constant to x * ( 1 / constant )) which changes the output
    struct BlockValues
    {
        const float* values;
        const float* constant;

        FS_FORCEINLINE float32v operator[]( intptr_t i ) const
        {
            return values ? FS::Load<float32v>( values + i ) : float32v( *constant );
        }
    };

    template<size_t DIMS, typename T>
    static FS_FORCEINLINE BlockValues GetSourceBlockValues( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, const float* pos, float* buffer, intptr_t count )
    {
        GetSourceBlock<DIMS>( memberVariable, seed, pos, buffer, count );
        return { buffer, nullptr };
    }

    template<size_t DIMS, typename T>
    static FS_FORCEINLINE BlockValues GetSourceBlockValues( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, const float* pos, float* buffer, intptr_t count )
    {
        if( memberVariable.simdGeneratorPtr )
        {
            CallSourceBlock<DIMS>( reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr ), seed, pos, buffer, count );
            return { buffer, nullptr };
        }
        return { nullptr, &memberVariable.constant };
    }

    // out = op( out, source values ) for a block
    template<size_t DIMS, typename S, typename OP>
    static FASTNOISE_NOINLINE void ApplySourceBlock( const S& memberVariable, OP op, int32v seed, const float* pos, float* out, intptr_t count )
    {
        alignas( 64 ) float buffer[kGenBlockSize];
        BlockValues values = GetSourceBlockValues<DIMS>( memberVariable, seed, pos, buffer, count );

        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            FS::Store( out + i, op( FS::Load<float32v>( out + i ), values[i] ) );
        }
    }

    // out = op( values of each source... ) for a block, the sources are evaluated in order a block at a time
    template<size_t DIMS, typename OP, typename... S>
    static FS_FORCEINLINE void CombineSourceBlocks( OP op, int32v seed, const float* pos, float* out, intptr_t count, const S&... sources )
    {
        CombineSourceBlocks<DIMS>( std::index_sequence_for<S...>(), op, seed, pos, out, count, sources... );
    }

    template<size_t DIMS, size_t... I, typename OP, typename... S>
    static FASTNOISE_NOINLINE void CombineSourceBlocks( std::index_sequence<I...>, OP& op, int32v seed, const float* pos, float* out, intptr_t count, const S&... sources )
    {
        // The first source is evaluated straight into out
        alignas( 64 ) float buffers[sizeof...( S ) > 1 ? sizeof...( S ) - 1 : 1][kGenBlockSize];

        const BlockValues values[] = { GetSourceBlockValues<DIMS>( sources, seed, pos, I == 0 ? out : buffers[I - ( I > 0 )], count )... };

        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            FS::Store( out + i, op( values[I][i]... ) );
        }
    }

protected:
    // Set by the constructor FASTNOISE_IMPL_GEN_T/FASTNOISE_IMPL_GEN_FN adds to each generator
    GenFn2D mGenFn2D = nullptr;
    GenFn3D mGenFn3D = nullptr;
    GenFn4D mGenFn4D = nullptr;
    GenBlockFn mGenBlockFn[3] = {};
    bool mHasGenBlock = false;

public:
    FastSIMD::FeatureSet GetActiveFeatureSet() const final
    {
        return FastSIMD::FeatureSetDefault();
    }

    using VoidPtrStorageType = const DispatchClass<Generator, SIMD>*;

    void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) final
    {
        if( !base )
        {
            *simdPtr = nullptr;
            return;
        }
        auto simd = dynamic_cast<VoidPtrStorageType>( base );

        assert( simd );
        *simdPtr = reinterpret_cast<const void*>( simd );
    }

    template<typename T, typename... POS>
    static FS_FORCEINLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

            return CallGen( simdGen, seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    static FS_FORCEINLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        return CallGen( simdGen, seed, pos... );
    }

    // Returns op( lhs, rhs ), evaluating LHS before RHS. A constant RHS gets its own path: there the positions are only
    // used by the LHS call, otherwise they are spilled before that call for an RHS call that doesn't happen.
    // Reading the constant before the call also keeps the compiler from hoisting the LHS call of both paths above the branch
    template<typename L, typename T, typename OP, typename... POS>
    static FS_FORCEINLINE float32v FS_VECTORCALL CombineSourceValues( const L& lhs, const FastNoise::HybridSourceT<T>& rhs, OP op, int32v seed, POS... pos )
    {
        if( !rhs.simdGeneratorPtr )
        {
            float rhsConstant = rhs.constant;

            return op( GetSourceValue( lhs, seed, pos... ), float32v( rhsConstant ) );
        }

        float32v lhsValue = GetSourceValue( lhs, seed, pos... );

        return op( lhsValue, GetSourceValue( rhs, seed, pos... ) );
    }

    template<typename T>
    static FS_FORCEINLINE const DispatchClass<T, SIMD>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable )
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<const FastSIMD::DispatchClass<T, SIMD>*>( simdGen );
        return simdT;
    }

    FastNoise::OutputMinMax GenUniformGrid2D( float* noiseOut, float xOffset, float yOffset, int xCount, int yCount, float xStepSize, float yStepSize, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        int32v xIdx( 0 );
        int32v yIdx( 0 );
        float32v xOffsetV( xOffset );
        float32v yOffsetV( yOffset );
        float32v xScale( xStepSize );
        float32v yScale( yStepSize );

        int32v xCountV( xCount );
        int32v xMax = xCountV + int32v( -1 );

        intptr_t totalValues = xCount * yCount;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xCountV, xCount );

        intptr_t xNextWrap = xCount;

        // yPos only changes when a lane wraps onto the next row
        float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

        alignas( 64 ) float posBuffer[kMinMaxBlockSize * 2];

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            float* blockPos = posBuffer;

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

                StoreBlockPositions( blockPos, xPos, yPos );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                if( AxisResetIfWrapped( xIdx, yIdx, xMax, xCountV, xCount, index, xNextWrap ) )
                {
                    yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );
                }
            }
            while( index < blockEnd );

            CallGenBlock<2>( this, int32v( seed ), posBuffer, &noiseOut[blockIndex], index - blockIndex );
            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid3D( float* noiseOut, float xOffset, float yOffset, float zOffset, int xCount, int yCount, int zCount, float xStepSize, float yStepSize, float zStepSize, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        int32v xIdx( 0 );
        int32v yIdx( 0 );
        int32v zIdx( 0 );
        float32v xOffsetV( xOffset );
        float32v yOffsetV( yOffset );
        float32v zOffsetV( zOffset );
        float32v xScale( xStepSize );
        float32v yScale( yStepSize );
        float32v zScale( zStepSize );

        int32v xCountV( xCount );
        int32v xMax = xCountV + int32v( -1 );
        int32v yCountV( yCount );
        int32v yMax = yCountV + int32v( -1 );

        intptr_t totalValues = xCount * yCount * zCount;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xCountV, xCount );
        AxisReset<true>( yIdx, zIdx, yMax, yCountV, xCount * yCount );

        intptr_t xNextWrap = xCount;
        intptr_t yNextWrap = xCount * yCount;

        // yPos/zPos only change when a lane wraps onto the next row/xy plane
        float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );
        float32v zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );

        alignas( 64 ) float posBuffer[kMinMaxBlockSize * 3];

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            float* blockPos = posBuffer;

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

                StoreBlockPositions( blockPos, xPos, yPos, zPos );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                // Every multiple of the y stride is a multiple of the x stride, so a y wrap is always also an x wrap
                if( AxisResetIfWrapped( xIdx, yIdx, xMax, xCountV, xCount, index, xNextWrap ) )
                {
                    if( AxisResetIfWrapped( yIdx, zIdx, yMax, yCountV, xCount * yCount, index, yNextWrap ) )
                    {
                        zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );
                    }
                    yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );
                }
            }
            while( index < blockEnd );

            CallGenBlock<3>( this, int32v( seed ), posBuffer, &noiseOut[blockIndex], index - blockIndex );
            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid4D( float* noiseOut, float xOffset, float yOffset, float zOffset, float wOffset, int xCount, int yCount, int zCount, int wCount, float xStepSize, float yStepSize, float zStepSize, float wStepSize, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        int32v xIdx( 0 );
        int32v yIdx( 0 );
        int32v zIdx( 0 );
        int32v wIdx( 0 );
        float32v xOffsetV( xOffset );
        float32v yOffsetV( yOffset );
        float32v zOffsetV( zOffset );
        float32v wOffsetV( wOffset );
        float32v xScale( xStepSize );
        float32v yScale( yStepSize );
        float32v zScale( zStepSize );
        float32v wScale( wStepSize );

        int32v xCountV( xCount );
        int32v xMax = xCountV + int32v( -1 );
        int32v yCountV( yCount );
        int32v yMax = yCountV + int32v( -1 );
        int32v zCountV( zCount );
        int32v zMax = zCountV + int32v( -1 );

        intptr_t totalValues = xCount * yCount * zCount * wCount;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xCountV, xCount );
        AxisReset<true>( yIdx, zIdx, yMax, yCountV, xCount * yCount );
        AxisReset<true>( zIdx, wIdx, zMax, zCountV, xCount * yCount * zCount );

        intptr_t yNextWrap = xCount * yCount;
        intptr_t zNextWrap = xCount * yCount * zCount;

        // zPos/wPos only change when a lane wraps onto the next xy plane/xyz volume
        float32v zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );
        float32v wPos = FS::FMulAdd( FS::Convert<float>( wIdx ), wScale, wOffsetV );

        alignas( 64 ) float posBuffer[kMinMaxBlockSize * 4];

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            float* blockPos = posBuffer;

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
                float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

                StoreBlockPositions( blockPos, xPos, yPos, zPos, wPos );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                // Unconditional as x wraps in (nearly) every iteration for typical 4D sizes, so yPos isn't kept either
                AxisReset<false>( xIdx, yIdx, xMax, xCountV, xCount );

                // Every multiple of the z stride is a multiple of the y stride, so a z wrap is always also a y wrap
                if( AxisResetIfWrapped( yIdx, zIdx, yMax, yCountV, xCount * yCount, index, yNextWrap ) )
                {
                    if( AxisResetIfWrapped( zIdx, wIdx, zMax, zCountV, xCount * yCount * zCount, index, zNextWrap ) )
                    {
                        wPos = FS::FMulAdd( FS::Convert<float>( wIdx ), wScale, wOffsetV );
                    }
                    zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );
                }
            }
            while( index < blockEnd );

            CallGenBlock<4>( this, int32v( seed ), posBuffer, &noiseOut[blockIndex], index - blockIndex );
            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
        float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray2D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, count - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
                float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );

                float32v gen = CallGen( this, int32v( seed ), xPos, yPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = float32v( xOffset ) + LoadRemaining( xPosArray, count, index );
        float32v yPos = float32v( yOffset ) + LoadRemaining( yPosArray, count, index );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray3D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, count - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
                float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
                float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );

                float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = float32v( xOffset ) + LoadRemaining( xPosArray, count, index );
        float32v yPos = float32v( yOffset ) + LoadRemaining( yPosArray, count, index );
        float32v zPos = float32v( zOffset ) + LoadRemaining( zPosArray, count, index );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray4D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, count - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
                float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
                float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );
                float32v wPos = float32v( wOffset ) + FS::Load<float32v>( &wPosArray[index] );

                float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos, wPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = float32v( xOffset ) + LoadRemaining( xPosArray, count, index );
        float32v yPos = float32v( yOffset ) + LoadRemaining( yPosArray, count, index );
        float32v zPos = float32v( zOffset ) + LoadRemaining( zPosArray, count, index );
        float32v wPos = float32v( wOffset ) + LoadRemaining( wPosArray, count, index );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    float GenSingle2D( float x, float y, int seed ) const final
    {
        return FS::Extract0( CallGen( this, int32v( seed ), float32v( x ), float32v( y ) ) );
    }

    float GenSingle3D( float x, float y, float z, int seed ) const final
    {
        return FS::Extract0( CallGen( this, int32v( seed ), float32v( x ), float32v( y ), float32v( z ) ) );
    }

    float GenSingle4D( float x, float y, float z, float w, int seed ) const final
    {
        return FS::Extract0( CallGen( this, int32v( seed ), float32v( x ), float32v( y ), float32v( z ), float32v( w ) ) );
    }

    FastNoise::OutputMinMax GenTileable2D( float* noiseOut, int xSize, int ySize, float xStepSize, float yStepSize, int seed ) const final
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        int32v xIdx( 0 );
        int32v yIdx( 0 );

        int32v xSizeV( xSize );
        int32v ySizeV( ySize );
        int32v xMax = xSizeV + int32v( -1 );

        intptr_t totalValues = xSize * ySize;
        intptr_t index = 0;

        float pi2Recip( 0.15915493667f );
        float xSizePi = (float)xSize * pi2Recip;
        float ySizePi = (float)ySize * pi2Recip;
        float32v xFreq = float32v( xSizePi * xStepSize );
        float32v yFreq = float32v( ySizePi * yStepSize );
        float32v xMul = float32v( 1 / xSizePi );
        float32v yMul = float32v( 1 / ySizePi );

        // xPos/zPos are the same in every row, compute them once per column. The tables continue past the
        // end of the row with its first columns, so the vector starting at any column is a contiguous load
        intptr_t xTableSize = xSize + (intptr_t)int32v::ElementCount;
        std::vector<float> xTables( ( xTableSize + int32v::ElementCount ) * 2 );
        float* xPosTable = xTables.data();
        float* zPosTable = xPosTable + xTableSize + int32v::ElementCount;

        for( intptr_t column = 0; column < xTableSize; column += int32v::ElementCount )
        {
            int32v columnIdx = int32v( (int32_t)column ) + FS::LoadIncremented<int32v>();

            for( mask32v wrap; FS::AnyMask( wrap = columnIdx > xMax ); )
            {
                columnIdx = FS::MaskedSub( wrap, columnIdx, xSizeV );
            }

            float32v xF = FS::Convert<float>( columnIdx ) * xMul;

            FS::Store( &xPosTable[column], FS::Cos( xF ) * xFreq );
            FS::Store( &zPosTable[column], FS::Sin( xF ) * xFreq );
        }

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        intptr_t xNextWrap = xSize;
        intptr_t xColumn = 0;

        // yPos/wPos only change when a lane wraps onto the next row
        float32v yF = FS::Convert<float>( yIdx ) * yMul;
        float32v yPos = FS::Cos( yF ) * yFreq;
        float32v wPos = FS::Sin( yF ) * yFreq;

        alignas( 64 ) float posBuffer[kMinMaxBlockSize * 4];

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            float* blockPos = posBuffer;

            do
            {
                float32v xPos = FS::Load<float32v>( &xPosTable[xColumn] );
                float32v zPos = FS::Load<float32v>( &zPosTable[xColumn] );

                StoreBlockPositions( blockPos, xPos, yPos, zPos, wPos );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                for( xColumn += int32v::ElementCount; xColumn >= xSize; xColumn -= xSize ) {}

                if( AxisResetIfWrapped( xIdx, yIdx, xMax, xSizeV, xSize, index, xNextWrap ) )
                {
                    yF = FS::Convert<float>( yIdx ) * yMul;
                    yPos = FS::Cos( yF ) * yFreq;
                    wPos = FS::Sin( yF ) * yFreq;
                }
            }
            while( index < blockEnd );

            CallGenBlock<4>( this, int32v( seed ), posBuffer, &noiseOut[blockIndex], index - blockIndex );
            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::Load<float32v>( &xPosTable[xColumn] );
        float32v zPos = FS::Load<float32v>( &zPosTable[xColumn] );

        float32v gen = CallGen( this, int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

private:
    template<bool INITIAL>
    static FS_FORCEINLINE void AxisReset( int32v& aIdx, int32v& bIdx, int32v aMax, int32v aSize, size_t aStep )
    {
        for( size_t resetLoop = INITIAL ? aStep : 0; resetLoop < int32v::ElementCount; resetLoop += aStep )
        {
            mask32v aReset = aIdx > aMax;
            bIdx = FS::MaskedIncrement( aReset, bIdx );
            aIdx = FS::MaskedSub( aReset, aIdx, aSize );
        }
    }

    // The reset is a no-op unless a lane crossed a multiple of aStep this iteration (lanes cover [index, index + ElementCount)),
    // nextWrap is the first multiple of aStep after the previous iteration's first lane. Returns true if the reset ran
    static FS_FORCEINLINE bool AxisResetIfWrapped( int32v& aIdx, int32v& bIdx, int32v aMax, int32v aSize, size_t aStep, intptr_t index, intptr_t& nextWrap )
    {
        if( index + (intptr_t)int32v::ElementCount > nextWrap )
        {
            AxisReset<false>( aIdx, bIdx, aMax, aSize, aStep );

            while( nextWrap <= index )
            {
                nextWrap += aStep;
            }
            return true;
        }
        return false;
    }

    // Min/max is accumulated from the stored output once per block of Gen() calls,
    // this avoids spilling and reloading the min/max registers around every call
    static constexpr intptr_t kMinMaxBlockSize = kGenBlockSize;

    // Positions are buffered per block and evaluated with one CallGenBlock(): the loops computing them keep their
    // state in registers instead of spilling it around every Gen call, and small generators inline into the block loop.
    // Not used for position arrays, they have no such state and copying their positions measured slower (AVX512)
    template<typename... P>
    static FS_FORCEINLINE void StoreBlockPositions( float*& blockPos, P... pos )
    {
        ( ( FS::Store( blockPos, pos ), blockPos += int32v::ElementCount ), ... );
    }

    static FS_FORCEINLINE void AccumulateMinMax( const float* noiseOut, intptr_t index, intptr_t endIndex, float32v& min, float32v& max )
    {
#if FASTNOISE_CALC_MIN_MAX
        for( ; index < endIndex; index += int32v::ElementCount )
        {
            float32v stored = FS::Load<float32v>( &noiseOut[index] );
            min = FS::Min( min, stored );
            max = FS::Max( max, stored );
        }
#endif
    }

    static FS_FORCEINLINE float32v LoadRemaining( const float* loadPtr, intptr_t totalValues, intptr_t index )        
    {
        if( index == 0 )
        {
            intptr_t remaining = totalValues - index;

            float32v load;
            std::memcpy( &load, loadPtr, remaining * sizeof( float ) );
            return load;
        }

        return FS::Load<float32v>( &loadPtr[totalValues - float32v::ElementCount] );
    }

    template<bool LOADREMAINING = false>
    static FS_FORCEINLINE FastNoise::OutputMinMax StoreRemaining( float* noiseOut, intptr_t totalValues, intptr_t index, float32v min, float32v max, float32v finalGen )
    {
        FastNoise::OutputMinMax minMax;
        intptr_t remaining = totalValues - index;

        if( LOADREMAINING ? index == 0 : remaining != (intptr_t)int32v::ElementCount )
        {
            std::memcpy( &noiseOut[index], &finalGen, remaining * sizeof( float ) );

#if FASTNOISE_CALC_MIN_MAX
            do
            {
                minMax << noiseOut[index];
            }
            while( ++index < totalValues );
#endif
        }
        else
        {
            FS::Store( &noiseOut[totalValues - float32v::ElementCount], finalGen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, finalGen );
            max = FS::Max( max, finalGen );
#endif
        }

#if FASTNOISE_CALC_MIN_MAX
        float* minP = reinterpret_cast<float*>(&min);
        float* maxP = reinterpret_cast<float*>(&max);
        for( size_t i = 0; i < int32v::ElementCount; i++ )
        {
            minMax << FastNoise::OutputMinMax{ minP[i], maxP[i] };
        }
#endif

        return minMax;
    }

    int32_t ReferencesFetchAdd( int32_t add ) const noexcept final
    {
        if( add )
        {
            return mReferences.fetch_add( add, std::memory_order_relaxed );
        }

        return mReferences.load( std::memory_order_relaxed );
    }
    
    mutable std::atomic<uint32_t> mReferences = 0;
};
