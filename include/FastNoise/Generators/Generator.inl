#include <cassert>
#include <cstring>

#include "Generator.h"

#pragma warning( disable:4250 )

using namespace FastNoise;

static constexpr size_t kRegisterSize = std::max<size_t>( 4, FS::NativeRegisterCount<float>() * 2 );
using float32v = FS::Register<float, kRegisterSize>;
using int32v = FS::Register<std::int32_t, kRegisterSize>;
using mask32v = typename float32v::MaskType;

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Generator, SIMD> : public virtual Generator
{
public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const { return Gen( seed, x, y, z ); }

#define FASTNOISE_IMPL_GEN_T\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const override { return GenT( seed, x, y ); }\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const override { return GenT( seed, x, y, z ); }\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const override { return GenT( seed, x, y, z, w ); }

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

            return simdGen->Gen( seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    static FS_FORCEINLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        return simdGen->Gen( seed, pos... );
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
        ScopeExitx86ZeroUpper zeroUpper;
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

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

                float32v gen = Gen( int32v( seed ), xPos, yPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                if( AxisResetIfWrapped( xIdx, yIdx, xMax, xCountV, xCount, index, xNextWrap ) )
                {
                    yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );
                }
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid3D( float* noiseOut, float xOffset, float yOffset, float zOffset, int xCount, int yCount, int zCount, float xStepSize, float yStepSize, float zStepSize, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

        intptr_t yNextWrap = xCount * yCount;

        // zPos only changes when a lane wraps onto the next xy plane
        float32v zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
                float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

                float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                AxisReset<false>( xIdx, yIdx, xMax, xCountV, xCount );

                if( AxisResetIfWrapped( yIdx, zIdx, yMax, yCountV, xCount * yCount, index, yNextWrap ) )
                {
                    zPos = FS::FMulAdd( FS::Convert<float>( zIdx ), zScale, zOffsetV );
                }
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
        float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid4D( float* noiseOut, float xOffset, float yOffset, float zOffset, float wOffset, int xCount, int yCount, int zCount, int wCount, float xStepSize, float yStepSize, float zStepSize, float wStepSize, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
                float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

                float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                AxisReset<false>( xIdx, yIdx, xMax, xCountV, xCount );

                // A z wrap is also a y wrap (zNextWrap is a multiple of yNextWrap)
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

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = FS::FMulAdd( FS::Convert<float>( xIdx ), xScale, xOffsetV );
        float32v yPos = FS::FMulAdd( FS::Convert<float>( yIdx ), yScale, yOffsetV );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray2D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

                float32v gen = Gen( int32v( seed ), xPos, yPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = float32v( xOffset ) + LoadRemaining( xPosArray, count, index );
        float32v yPos = float32v( yOffset ) + LoadRemaining( yPosArray, count, index );

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray3D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

                float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xPos = float32v( xOffset ) + LoadRemaining( xPosArray, count, index );
        float32v yPos = float32v( yOffset ) + LoadRemaining( yPosArray, count, index );
        float32v zPos = float32v( zOffset ) + LoadRemaining( zPosArray, count, index );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray4D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

                float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
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

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining<true>( noiseOut, count, index, min, max, gen );
    }

    float GenSingle2D( float x, float y, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ) ) );
    }

    float GenSingle3D( float x, float y, float z, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ) ) );
    }

    float GenSingle4D( float x, float y, float z, float w, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ), float32v( w ) ) );
    }

    FastNoise::OutputMinMax GenTileable2D( float* noiseOut, int xSize, int ySize, float xStepSize, float yStepSize, int seed ) const final
    {
        ScopeExitx86ZeroUpper zeroUpper;
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

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            intptr_t blockIndex = index;
            intptr_t blockEnd = std::min<intptr_t>( index + kMinMaxBlockSize, totalValues - (intptr_t)int32v::ElementCount );

            do
            {
                float32v xF = FS::Convert<float>( xIdx ) * xMul;
                float32v yF = FS::Convert<float>( yIdx ) * yMul;

                float32v xPos = FS::Cos( xF ) * xFreq;
                float32v yPos = FS::Cos( yF ) * yFreq;
                float32v zPos = FS::Sin( xF ) * xFreq;
                float32v wPos = FS::Sin( yF ) * yFreq;

                float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
                FS::Store( &noiseOut[index], gen );

                index += int32v::ElementCount;
                xIdx += int32v( int32v::ElementCount );

                AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            }
            while( index < blockEnd );

            AccumulateMinMax( noiseOut, blockIndex, index, min, max );
        }

        float32v xF = FS::Convert<float>( xIdx ) * xMul;
        float32v yF = FS::Convert<float>( yIdx ) * yMul;

        float32v xPos = FS::Cos( xF ) * xFreq;
        float32v yPos = FS::Cos( yF ) * yFreq;
        float32v zPos = FS::Sin( xF ) * xFreq;
        float32v wPos = FS::Sin( yF ) * yFreq;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return StoreRemaining( noiseOut, totalValues, index, min, max, gen );
    }

private:
    struct ScopeExitx86ZeroUpper
    {
        FS_FORCEINLINE ~ScopeExitx86ZeroUpper()
        {
            if constexpr( SIMD & FeatureFlag::AVX )
            {
                FS_BIND_INTRINSIC( _mm256_zeroupper )();
            }
        }
    };

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
