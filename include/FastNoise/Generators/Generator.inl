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
    using GenBlockFn = void( FS_VECTORCALL* )( const DispatchClass*, int32v, const float* __restrict, float* __restrict, intptr_t );

    template<size_t DIMS>
    static FS_FORCEINLINE void CallGenBlock( const DispatchClass* gen, int32v seed, const float* pos, float* out, intptr_t count )
    {
        assert( gen->mGenBlockFn[DIMS - 2] );
        gen->mGenBlockFn[DIMS - 2]( gen, seed, pos, out, count );
    }

    // Evaluates rows of xCount values (a multiple of N) on a uniform grid: x from the column table xTable, the other
    // positions are constant per row and stored as DIMS - 1 vectors per row in rowPos. rowPos and out aren't restrict:
    // loading the row positions per vector keeps the inlined generator's fast-math expressions as in GenBlockFn
    using GenGridBlockFn = void( FS_VECTORCALL* )( const DispatchClass* __restrict, int32v, const float* __restrict, const float*, intptr_t, intptr_t, float* );

    template<size_t DIMS>
    static FS_FORCEINLINE void CallGenGridBlock( const DispatchClass* gen, int32v seed, const float* xTable, const float* rowPos, intptr_t rows, intptr_t xCount, float* out )
    {
        assert( gen->mGenGridBlockFn[DIMS - 2] );
        gen->mGenGridBlockFn[DIMS - 2]( gen, seed, xTable, rowPos, rows, xCount, out );
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
    FASTNOISE_GEN_FN_ATTRIBUTES static void FS_VECTORCALL GenBlockFn( const DispatchClass<Generator, SIMD>* gen, int32v seed, const float* __restrict pos, float* __restrict out, intptr_t count )\
    {\
        for( intptr_t i = 0; i < count; i += int32v::ElementCount )\
        {\
            GenResultT result = GenFn( gen, seed, FS::Load<float32v>( pos + i * sizeof...( DIM ) + DIM * int32v::ElementCount )... );\
            FS::Store( out + i, float32v( result.v0, result.v1 ) );\
        }\
    }\
    template<size_t... DIM>\
    FASTNOISE_GEN_FN_ATTRIBUTES static void FS_VECTORCALL GenGridBlockFn( const DispatchClass<Generator, SIMD>* __restrict gen, int32v seed, const float* __restrict xTable, const float* rowPos, intptr_t rows, intptr_t xCount, float* out )\
    {\
        for( ; rows > 0; rows--, rowPos += sizeof...( DIM ) * int32v::ElementCount, out += xCount )\
        {\
            for( intptr_t i = 0; i < xCount; i += int32v::ElementCount )\
            {\
                GenResultT result = GenFn( gen, seed, FS::Load<float32v>( xTable + i ), FS::Load<float32v>( rowPos + DIM * int32v::ElementCount )... );\
                FS::Store( out + i, float32v( result.v0, result.v1 ) );\
            }\
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
        this->mGenGridBlockFn[0] = &GenGridBlockFn<0>;\
        this->mGenGridBlockFn[1] = &GenGridBlockFn<0, 1>;\
        this->mGenGridBlockFn[2] = &GenGridBlockFn<0, 1, 2>;\
    }\
private:

#define FASTNOISE_IMPL_GEN_T FASTNOISE_IMPL_GEN_FN( GenT )

protected:
    // Set by the constructor FASTNOISE_IMPL_GEN_T/FASTNOISE_IMPL_GEN_FN adds to each generator
    GenFn2D mGenFn2D = nullptr;
    GenFn3D mGenFn3D = nullptr;
    GenFn4D mGenFn4D = nullptr;
    GenBlockFn mGenBlockFn[3] = {};
    GenGridBlockFn mGenGridBlockFn[3] = {};

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
        if( IsGridRowsSize( xCount ) && yCount > 0 )
        {
            const int count[] = { xCount, yCount };
            const float offset[] = { xOffset, yOffset };
            const float stepSize[] = { xStepSize, yStepSize };
            return GenUniformGridRows<2>( noiseOut, count, offset, stepSize, seed );
        }

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
        if( IsGridRowsSize( xCount ) && yCount > 0 && zCount > 0 )
        {
            const int count[] = { xCount, yCount, zCount };
            const float offset[] = { xOffset, yOffset, zOffset };
            const float stepSize[] = { xStepSize, yStepSize, zStepSize };
            return GenUniformGridRows<3>( noiseOut, count, offset, stepSize, seed );
        }

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
        if( IsGridRowsSize( xCount ) && yCount > 0 && zCount > 0 && wCount > 0 )
        {
            const int count[] = { xCount, yCount, zCount, wCount };
            const float offset[] = { xOffset, yOffset, zOffset, wOffset };
            const float stepSize[] = { xStepSize, yStepSize, zStepSize, wStepSize };
            return GenUniformGridRows<4>( noiseOut, count, offset, stepSize, seed );
        }

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
    // Longest row for the uniform grid row path, its x table is on the stack
    static constexpr intptr_t kGridRowsMaxXCount = 2048;

    static FS_FORCEINLINE bool IsGridRowsSize( int xCount )
    {
        return xCount > 0 && xCount <= kGridRowsMaxXCount && xCount % (int)int32v::ElementCount == 0;
    }

    // Uniform grid with rows of whole vectors: x is the same in every row and comes from a column table, the other
    // positions are constant per row, so no positions are stored per vector. Every position is the same vector
    // FMulAdd of the same index as in the per vector loop, the output is identical
    template<size_t DIMS>
    FastNoise::OutputMinMax GenUniformGridRows( float* noiseOut, const int* count, const float* offset, const float* stepSize, int seed ) const
    {
        float32v min( kInfinity );
        float32v max( -kInfinity );

        intptr_t xCount = count[0];
        intptr_t totalRows = 1;

        for( size_t dim = 1; dim < DIMS; dim++ )
        {
            totalRows *= count[dim];
        }

        alignas( 64 ) float xTable[kGridRowsMaxXCount];
        {
            float32v xScale( stepSize[0] );
            float32v xOffsetV( offset[0] );

            for( intptr_t column = 0; column < xCount; column += int32v::ElementCount )
            {
                int32v xIdx = int32v( (int32_t)column ) + FS::LoadIncremented<int32v>();

                FS::Store( &xTable[column], FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV ) );
            }
        }

        // Whole rows per block, at least one
        constexpr intptr_t kMaxBlockRows = kMinMaxBlockSize / (intptr_t)int32v::ElementCount;
        intptr_t blockRows = std::max<intptr_t>( 1, kMinMaxBlockSize / xCount );

        alignas( 64 ) float rowPos[kMaxBlockRows * ( DIMS - 1 ) * int32v::ElementCount];
        int32_t rowIdx[DIMS - 1] = {};

        intptr_t blockIndex;
        intptr_t rows;

        for( intptr_t row = 0;; )
        {
            rows = std::min<intptr_t>( blockRows, totalRows - row );

            for( intptr_t blockRow = 0; blockRow < rows; blockRow++ )
            {
                for( size_t dim = 1; dim < DIMS; dim++ )
                {
                    float32v pos = FS::FMulAdd( FS::Convert<float>( int32v( rowIdx[dim - 1] ) ), float32v( stepSize[dim] ), float32v( offset[dim] ) );

                    FS::Store( &rowPos[( blockRow * ( DIMS - 1 ) + dim - 1 ) * int32v::ElementCount], pos );
                }

                // Next row: increment y, carrying into z and w
                for( size_t dim = 1; dim < DIMS && ++rowIdx[dim - 1] == count[dim]; dim++ )
                {
                    rowIdx[dim - 1] = 0;
                }
            }

            blockIndex = row * xCount;
            row += rows;

            if( row == totalRows )
            {
                break;
            }

            CallGenGridBlock<DIMS>( this, int32v( seed ), xTable, rowPos, rows, xCount, &noiseOut[blockIndex] );
            AccumulateMinMax( noiseOut, blockIndex, row * xCount, min, max );
        }

        // The last vector of the last block is generated by CallGen and stored by StoreRemaining as in the other paths
        intptr_t lastIndex = xCount * totalRows - (intptr_t)int32v::ElementCount;
        const float* lastRowPos = &rowPos[( rows - 1 ) * ( DIMS - 1 ) * int32v::ElementCount];

        CallGenGridBlock<DIMS>( this, int32v( seed ), xTable, rowPos, rows - 1, xCount, &noiseOut[blockIndex] );
        CallGenGridBlock<DIMS>( this, int32v( seed ), xTable, lastRowPos, 1, xCount - (intptr_t)int32v::ElementCount, &noiseOut[lastIndex + (intptr_t)int32v::ElementCount - xCount] );
        AccumulateMinMax( noiseOut, blockIndex, lastIndex, min, max );

        float32v gen = CallGenGridRow( std::make_index_sequence<DIMS - 1>(), seed, &xTable[xCount - (intptr_t)int32v::ElementCount], lastRowPos );

        return StoreRemaining( noiseOut, lastIndex + (intptr_t)int32v::ElementCount, lastIndex, min, max, gen );
    }

    template<size_t... I>
    FS_FORCEINLINE float32v CallGenGridRow( std::index_sequence<I...>, int seed, const float* xPos, const float* rowPos ) const
    {
        return CallGen( this, int32v( seed ), FS::Load<float32v>( xPos ), FS::Load<float32v>( &rowPos[I * int32v::ElementCount] )... );
    }

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
    static constexpr intptr_t kMinMaxBlockSize = int32v::ElementCount * 16;

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
