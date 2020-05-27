#include <cassert>
#include "FastSIMD/InlInclude.h"

#include "Generator.h"

#ifdef FS_SIMD_CLASS
#pragma warning( disable:4250 )
#endif

template<typename FS>
class FS_T<FastNoise::Generator, FS> : public virtual FastNoise::Generator
{
public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const = 0;

#define FASTNOISE_IMPL_GEN_T\
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const override { return GenT( seed, x, y ); }\
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const override { return GenT( seed, x, y, z ); }

    FastSIMD::eLevel GetSIMDLevel() const final
    {
        return FS::SIMD_Level;
    }

    using VoidPtrStorageType = FS_T<Generator, FS>*;

    void SetSourceSIMDPtr( Generator* base, void** simdPtr ) final
    {
        auto simd = dynamic_cast<VoidPtrStorageType>( base );
        assert( simd );
        *simdPtr = reinterpret_cast<void*>( simd );
    }

    template<typename T, typename... POS>
    FS_INLINE float32v FS_VECTORCALL GetSourceValue( const HybridSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

            auto simdT = static_cast<FS_T<T, FS>*>( simdGen );
            return simdT->Gen( seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    FS_INLINE float32v FS_VECTORCALL GetSourceValue( const GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<FS_T<T, FS>*>( simdGen );
        return simdT->Gen( seed, pos... );
    }

    template<typename T>
    FS_INLINE const FS_T<T, FS>* GetSourceSIMD( const GeneratorSourceT<T>& memberVariable ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<FS_T<T, FS>*>( simdGen );
        return simdT;
    }

    void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) const final
    {
        int32v xIdx = int32v::FS_Zero();
        int32v yIdx = int32v::FS_Incremented();

        float32v xOffset( xStart );
        float32v yOffset( yStart );

        float32v xScale( xStep );
        float32v yScale( yStep );

        int32v ySizeV( ySize );
        int32v yMax = ySizeV + int32v( -1 );

        int32_t totalValues = xSize * ySize;
        int32_t index = 0;

        while ( index < totalValues - float32v::FS_Size() )
        {
            float32v xPos = xOffset + (FS_Converti32_f32( xIdx ) * xScale);
            float32v yPos = yOffset + (FS_Converti32_f32( yIdx ) * yScale);

            FS_Store_f32( &noiseOut[index], Gen( int32v( seed ), xPos, yPos ));
            index += float32v::FS_Size();

            yIdx += int32v( int32v::FS_Size() );

            mask32v yReset = FS_GreaterThan_i32( yIdx, yMax );
            xIdx = FS_MaskedIncrement_i32( xIdx, yReset );
            yIdx = FS_MaskedSub_i32( yIdx, ySizeV, yReset );
        }

        float32v xPos = xOffset + (FS_Converti32_f32( xIdx ) * xScale);
        float32v yPos = yOffset + (FS_Converti32_f32( yIdx ) * yScale);

        float32v gen = Gen( int32v( seed ), xPos, yPos );
        int32_t remaining = totalValues - index;

        switch ( remaining )
        {
            case float32v::FS_Size():
            FS_Store_f32( &noiseOut[index], gen );
            break;

        default:
            memcpy( &noiseOut[index], &gen, size_t( remaining ) * sizeof( int32_t ) );
            break;
        }
    }

    void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) const final
    {
        int32v xIdx( 0 );
        int32v yIdx( 0 );
        int32v zIdx = int32v::FS_Incremented();

        float32v xOffset( xStart );
        float32v yOffset( yStart );
        float32v zOffset( zStart );

        float32v xScale( xStep );
        float32v yScale( yStep );
        float32v zScale( zStep );

        int32v ySizeV( ySize );
        int32v yMax = ySizeV + int32v( -1 );
        int32v zSizeV( zSize );
        int32v zMax = zSizeV + int32v( -1 );

        int32_t totalValues = xSize * ySize * zSize;
        int32_t index = 0;

        while ( index < totalValues - float32v::FS_Size() )
        {
            float32v xPos = xOffset + (FS_Converti32_f32( xIdx ) * xScale);
            float32v yPos = yOffset + (FS_Converti32_f32( yIdx ) * yScale);
            float32v zPos = zOffset + (FS_Converti32_f32( zIdx ) * zScale);

            FS_Store_f32( &noiseOut[index], Gen( int32v( seed ), xPos, yPos, zPos ) );
            index += float32v::FS_Size();

            zIdx += int32v( int32v::FS_Size() );

            mask32v zReset = FS_GreaterThan_i32( zIdx, zMax );
            yIdx = FS_MaskedIncrement_i32( yIdx, zReset );
            zIdx = FS_MaskedSub_i32( zIdx, zSizeV, zReset );

            mask32v yReset = FS_GreaterThan_i32( yIdx, yMax );
            xIdx = FS_MaskedIncrement_i32( xIdx, yReset );
            yIdx = FS_MaskedSub_i32( yIdx, ySizeV, yReset );
        }

        float32v xPos = xOffset + (FS_Converti32_f32( xIdx ) * xScale);
        float32v yPos = yOffset + (FS_Converti32_f32( yIdx ) * yScale);
        float32v zPos = zOffset + (FS_Converti32_f32( zIdx ) * zScale);

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
        int32_t remaining = totalValues - index;

        switch ( remaining )
        {
            case float32v::FS_Size():
                FS_Store_f32( &noiseOut[index], gen );
                break;

            default:
                memcpy( &noiseOut[index], &gen, size_t( remaining ) * sizeof( int32_t ) );
                break;
        }
    }

    void GenPositionArray3D( float* noiseOut, const float* xPosArray, const float* yPosArray, const float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) const final
    {
        int32_t index = 0;
        while( index < int64_t(count) - float32v::FS_Size() )
        {
            float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );
            float32v zPos = float32v( zOffset ) + FS_Load_f32( &zPosArray[index] );

            FS_Store_f32( &noiseOut[index], Gen( int32v( seed ), xPos, yPos, zPos ) );
            index += float32v::FS_Size();
        }

        float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );
        float32v zPos = float32v( zOffset ) + FS_Load_f32( &zPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
        int32_t remaining = count - index;

        switch( remaining )
        {
        case float32v::FS_Size():
            FS_Store_f32( &noiseOut[index], gen );
            break;

        default:
            memcpy( &noiseOut[index], &gen, remaining * sizeof( int32_t ) );
            break;
        }
    }
};


template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};
