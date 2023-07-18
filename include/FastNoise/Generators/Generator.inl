#include <cassert>
#include <cstring>

#include "Generator.h"

#pragma warning( disable:4250 )

using namespace FastNoise;

using float32v = FS::Register<float, NativeRegisterCount<float>() * 2>;
using int32v = FS::Register<std::int32_t, NativeRegisterCount<std::int32_t>() * 2>;
using mask32v = typename float32v::MaskType;

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Generator, SIMD> : public virtual FastNoise::Generator
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
    FS_FORCEINLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

            return simdGen->Gen( seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    FS_FORCEINLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        return simdGen->Gen( seed, pos... );
    }

    template<typename T>
    FS_FORCEINLINE const DispatchClass<T, SIMD>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<const FastSIMD::DispatchClass<T, SIMD>*>( simdGen );
        return simdT;
    }

    FastNoise::OutputMinMax GenUniformGrid2D( float* noiseOut, int xStart, int yStart, int xSize, int ySize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = FS::Convert<float>( xIdx ) * freqV;
            float32v yPos = FS::Convert<float>( yIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif

            index += int32v::ElementCount;
            xIdx += int32v( int32v::ElementCount );

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
        }

        float32v xPos = FS::Convert<float>( xIdx ) * freqV;
        float32v yPos = FS::Convert<float>( yIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid3D( float* noiseOut, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );
        int32v zIdx( zStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );
        int32v ySizeV( ySize );
        int32v yMax = ySizeV + yIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize * zSize;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );
        AxisReset<true>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = FS::Convert<float>( xIdx ) * freqV;
            float32v yPos = FS::Convert<float>( yIdx ) * freqV;
            float32v zPos = FS::Convert<float>( zIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif

            index += int32v::ElementCount;
            xIdx += int32v( int32v::ElementCount );
            
            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            AxisReset<false>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
        }

        float32v xPos = FS::Convert<float>( xIdx ) * freqV;
        float32v yPos = FS::Convert<float>( yIdx ) * freqV;
        float32v zPos = FS::Convert<float>( zIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid4D( float* noiseOut, int xStart, int yStart, int zStart, int wStart, int xSize, int ySize, int zSize, int wSize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );
        int32v zIdx( zStart );
        int32v wIdx( wStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );
        int32v ySizeV( ySize );
        int32v yMax = ySizeV + yIdx + int32v( -1 );
        int32v zSizeV( zSize );
        int32v zMax = zSizeV + zIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize * zSize * wSize;
        intptr_t index = 0;

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );
        AxisReset<true>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
        AxisReset<true>( zIdx, wIdx, zMax, zSizeV, xSize * ySize * zSize );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = FS::Convert<float>( xIdx ) * freqV;
            float32v yPos = FS::Convert<float>( yIdx ) * freqV;
            float32v zPos = FS::Convert<float>( zIdx ) * freqV;
            float32v wPos = FS::Convert<float>( wIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif

            index += int32v::ElementCount;
            xIdx += int32v( int32v::ElementCount );

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            AxisReset<false>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
            AxisReset<false>( zIdx, wIdx, zMax, zSizeV, xSize * ySize * zSize );
        }

        float32v xPos = FS::Convert<float>( xIdx ) * freqV;
        float32v yPos = FS::Convert<float>( yIdx ) * freqV;
        float32v zPos = FS::Convert<float>( zIdx ) * freqV;
        float32v wPos = FS::Convert<float>( wIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray2D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif
            index += int32v::ElementCount;
        }

        float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray3D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
            float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif
            index += int32v::ElementCount;
        }

        float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
        float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray4D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)int32v::ElementCount )
        {
            float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
            float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );
            float32v wPos = float32v( wOffset ) + FS::Load<float32v>( &wPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif
            index += int32v::ElementCount;
        }

        float32v xPos = float32v( xOffset ) + FS::Load<float32v>( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS::Load<float32v>( &yPosArray[index] );
        float32v zPos = float32v( zOffset ) + FS::Load<float32v>( &zPosArray[index] );
        float32v wPos = float32v( wOffset ) + FS::Load<float32v>( &wPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    float GenSingle2D( float x, float y, int seed ) const final
    {
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ) ) );
    }

    float GenSingle3D( float x, float y, float z, int seed ) const final
    {
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ) ) );
    }

    float GenSingle4D( float x, float y, float z, float w, int seed ) const final
    {
        return FS::Extract0( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ), float32v( w ) ) );
    }

    FastNoise::OutputMinMax GenTileable2D( float* noiseOut, int xSize, int ySize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( 0 );
        int32v yIdx( 0 );

        int32v xSizeV( xSize );
        int32v ySizeV( ySize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize;
        intptr_t index = 0;

        float pi2Recip( 0.15915493667f );
        float xSizePi = (float)xSize * pi2Recip;
        float ySizePi = (float)ySize * pi2Recip;
        float32v xFreq = float32v( frequency * xSizePi );
        float32v yFreq = float32v( frequency * ySizePi );
        float32v xMul = float32v( 1 / xSizePi );
        float32v yMul = float32v( 1 / ySizePi );

        xIdx += FS::LoadIncremented<int32v>();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        while( index < totalValues - (intptr_t)int32v::ElementCount )
        {
            float32v xF = FS::Convert<float>( xIdx ) * xMul;
            float32v yF = FS::Convert<float>( yIdx ) * yMul;

            float32v xPos = FS::Cos( xF ) * xFreq;
            float32v yPos = FS::Cos( yF ) * yFreq;
            float32v zPos = FS::Sin( xF ) * xFreq;
            float32v wPos = FS::Sin( yF ) * yFreq;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
            FS::Store( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, gen );
            max = FS::Max( max, gen );
#endif

            index += int32v::ElementCount;
            xIdx += int32v( int32v::ElementCount );

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
        }

        float32v xF = FS::Convert<float>( xIdx ) * xMul;
        float32v yF = FS::Convert<float>( yIdx ) * yMul;

        float32v xPos = FS::Cos( xF ) * xFreq;
        float32v yPos = FS::Cos( yF ) * yFreq;
        float32v zPos = FS::Sin( xF ) * xFreq;
        float32v wPos = FS::Sin( yF ) * yFreq;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
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

    static FS_FORCEINLINE FastNoise::OutputMinMax DoRemaining( float* noiseOut, intptr_t totalValues, intptr_t index, float32v min, float32v max, float32v finalGen )
    {
        FastNoise::OutputMinMax minMax;
        intptr_t remaining = totalValues - index;

        if( remaining == (intptr_t)int32v::ElementCount )
        {
            FS::Store( &noiseOut[index], finalGen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS::Min( min, finalGen );
            max = FS::Max( max, finalGen );
#endif
        }
        else
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
