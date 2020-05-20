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
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) = 0;

#define FASTNOISE_IMPL_GEN_T\
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override { return GenT( seed, x, y ); }\
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override { return GenT( seed, x, y, z ); }

    virtual FastSIMD::eLevel GetSIMDLevel() final
    {
        return FS::SIMD_Level;
    }

    void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) final
    {
        int32v xIdx = int32v::FS_Zero();
        int32v yIdx = int32v::FS_Incremented();

        float32v xOffset = float32v( xStart );
        float32v yOffset = float32v( yStart );

        float32v xScale = float32v( xStep );
        float32v yScale = float32v( yStep );

        int32v ySizeV = int32v( ySize );
        int32v yMax = int32v( ySize ) + int32v( -1 );

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

    void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) final
    {
        int32v xIdx = int32v::FS_Zero();
        int32v yIdx = int32v::FS_Zero();
        int32v zIdx = int32v::FS_Incremented();

        float32v xOffset = float32v( xStart );
        float32v yOffset = float32v( yStart );
        float32v zOffset = float32v( zStart );

        float32v xScale = float32v( xStep );
        float32v yScale = float32v( yStep );
        float32v zScale = float32v( zStep );

        int32v ySizeV = int32v( ySize );
        int32v yMax = int32v( ySize ) + int32v( -1 );
        int32v zSizeV = int32v( zSize );
        int32v zMax = int32v( zSize ) + int32v( -1 );

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

    void GenPositionArray3D( float* noiseOut, const float* xPosArray, const float* yPosArray, const float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) final
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

template<typename FS, auto SOURCE_COUNT, typename T, typename P>
class FS_T<FastNoise::SourceStore<SOURCE_COUNT, T, P>, FS> : public virtual FastNoise::SourceStore<SOURCE_COUNT, T, P>, public FS_T<P, FS>
{
public:
    template<size_t index, typename... ARGS>
    FS_INLINE float32v GetSourceValue( const HybridSource<index>& value, int32v seed, ARGS&&... pos )
    {
        static_assert( index < SOURCE_COUNT );

        if( mSourceSIMD[index] ) return mSourceSIMD[index]->Gen( seed, pos... );

        return float32v( value.constant );
    }

    template<size_t index, typename... ARGS>
    FS_INLINE float32v GetSourceValue( const GeneratorSource<index>& value, int32v seed, ARGS&&... pos )
    {
        static_assert( index < SOURCE_COUNT );
        assert( mSourceSIMD[index] );

        return mSourceSIMD[index]->Gen( seed, pos... );
    }

    template<size_t index>
    FS_INLINE FS_T<T, FS>* GetSourceSIMD( const GeneratorSource<index>& value )
    {
        static_assert( index < SOURCE_COUNT );
        assert( mSourceSIMD[index] );

        return mSourceSIMD[index];
    }

private:
    void SetSourceImpl( const std::shared_ptr<T>& gen, size_t index ) final
    {
        assert( gen->GetSIMDLevel() == this->GetSIMDLevel() );

        if( index < SOURCE_COUNT && gen->GetSIMDLevel() == this->GetSIMDLevel() )
        {
            this->mSourceBase[index] = gen;
            mSourceSIMD[index] = dynamic_cast<FS_T<T, FS>*>( gen.get() );
            assert( mSourceSIMD[index] );
        }
    }

    std::array<std::shared_ptr<T>, SOURCE_COUNT> mSourceBase;
    std::array<FS_T<T, FS>*, SOURCE_COUNT> mSourceSIMD = {};
};


template<typename FS, typename T, typename P>
class FS_T<FastNoise::SingleSource<T, P>, FS> : public virtual FastNoise::SingleSource<T, P>, public FS_T<FastNoise::SourceStore<1, T, P>, FS>
{

};

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    float32v FS_INLINE GenT( int32v seed, P... pos )
    {
        return float32v( mValue );
    }
};
