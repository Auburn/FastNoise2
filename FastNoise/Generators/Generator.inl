#include "Generator.h"
#include "../FastSIMD/Internal/FunctionList.h"

#include <cassert>

namespace FastNoise
{
    namespace Primes
    {
        static const int32_t X = 1619;
        static const int32_t Y = 31337;
        static const int32_t Z = 6971;
        static const int32_t W = 1013;

        static const int32_t Lookup[] = { X,Y,Z,W };
    }

    template<typename FS>
    class Generator_FS : public virtual Generator
    {
    public:
        typedef typename FS::float32v float32v;
        typedef typename FS::int32v int32v;
        typedef typename FS::mask32v mask32v;

        virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) = 0;
        virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) = 0;

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

                FS_Store_f32( &noiseOut[index], Gen( int32v( seed ), xPos, yPos ) );
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

        template<typename FS = FS>
        FS_INLINE typename std::enable_if_t<( FS::VectorSize < 32 ), float32v> GetGradientDot( int32v hash, float32v fX, float32v fY )
        {
            // ( 0, 1) (-1, 0) ( 0,-1) ( 1, 0)
            // ( 1, 1) (-1, 1) (-1,-1) ( 1,-1)
        
            int32v bit1 = (hash << 31);
            int32v bit2 = (hash >> 1) << 31;
            int32v bit4 = (hash << 29);
        
            fX = FS_BitwiseXor_f32( fX, FS_Casti32_f32( bit1 ^ bit2 ) );
            fY = FS_BitwiseXor_f32( fY, FS_Casti32_f32( bit2 ) );
        
            int32v zeroX = bit1;
            int32v zeroY = ~zeroX;
        
            fX = FS_BitwiseAnd_f32( fX, FS_Casti32_f32( (zeroX | bit4) >> 31 ) );
            fY = FS_BitwiseAnd_f32( fY, FS_Casti32_f32( (zeroY | bit4) >> 31 ) );
        
            return fX + fY;
        }

        template<typename FS = FS>
        FS_INLINE typename std::enable_if_t<( FS::SIMD_Level == FastSIMD::Level_AVX2 ), float32v> GetGradientDot( int32v hash, float32v fX, float32v fY )
        {
            // ( 0, 1) (-1, 0) ( 0,-1) ( 1, 0)
            // ( 1, 1) (-1, 1) (-1,-1) ( 1,-1)
        
            float32v gX = _mm256_permutevar8x32_ps( float32v( 0, -1, 0, 1, 1, -1, -1, 1 ), hash );
            float32v gY = _mm256_permutevar8x32_ps( float32v( 1, 0, -1, 0, 1, 1, -1, -1 ), hash );
        
            return FS_FMulAdd_f32( gX, fX, fY * gY );
        }

        template<typename FS = FS>
        FS_INLINE typename std::enable_if_t<( FS::SIMD_Level == FastSIMD::Level_AVX512 ), float32v> GetGradientDot( int32v hash, float32v fX, float32v fY )
        {
            // ( 0, 1) (-1, 0) ( 0,-1) ( 1, 0)
            // ( 1, 1) (-1, 1) (-1,-1) ( 1,-1)
        
            float32v gX = _mm512_permutexvar_ps( hash, float32v( 0, -1, 0, 1, 1, -1, -1, 1, 0, -1, 0, 1, 1, -1, -1, 1 ) );
            float32v gY = _mm512_permutexvar_ps( hash, float32v( 1, 0, -1, 0, 1, 1, -1, -1, 1, 0, -1, 0, 1, 1, -1, -1 ) );
        
            return FS_FMulAdd_f32( gX, fX, fY * gY );
        }

        template<typename FS = FS>
        FS_INLINE typename std::enable_if_t<( FS::VectorSize < 64 ), float32v> GetGradientDot( int32v hash, float32v fX, float32v fY, float32v fZ  )
        {
            int32v hasha13 = hash & int32v( 13 );

            //if h < 8 then x, else y
            mask32v l8 = FS_LessThan_i32( hasha13, int32v( 8 ) );
            float32v u = FS_Select_f32( l8, fX, fY );

            //if h < 4 then y else if h is 12 or 14 then x else z
            mask32v l4 = FS_LessThan_i32( hasha13, int32v( 2 ) );
            mask32v h12o14 = FS_Equal_i32( hasha13, int32v( 12 ) );
            float32v v = FS_Select_f32( l4, fY, FS_Select_f32( h12o14, fX, fZ ) );

            //if h1 then -u else u
            //if h2 then -v else v
            float32v h1 = FS_Casti32_f32( hash << 31 );
            float32v h2 = FS_Casti32_f32( (hash & int32v( 2 )) << 30 );
            //then add them
            return FS_BitwiseXor_f32( u, h1 ) + FS_BitwiseXor_f32( v, h2 );
        }

        template<typename FS = FS>
        FS_INLINE typename std::enable_if_t<( FS::SIMD_Level == FastSIMD::Level_AVX512 ), float32v> GetGradientDot( int32v hash, float32v fX, float32v fY, float32v fZ )
        {
            float32v gX = _mm512_permutexvar_ps( hash, float32v( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 ) );
            float32v gY = _mm512_permutexvar_ps( hash, float32v( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            float32v gZ = _mm512_permutexvar_ps( hash, float32v( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 ) );
        
            return FS_FMulAdd_f32( gX, fX, FS_FMulAdd_f32( fY, gY, fZ * gZ ) );
        }

        template<typename... P>
        FS_INLINE int32v HashPrimes( int32v seed, P... primedPos )
        {
            int32v hash = seed;
            hash ^= (primedPos ^ ...);

            hash = hash * hash * hash * int32v( 60493 );
            return (hash >> 13) ^ hash;
        }
    };

    template<typename FS, size_t SOURCE_COUNT>
    class Modifier_FS : private virtual Modifier<SOURCE_COUNT>, public Generator_FS<FS>
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen, size_t index ) final
        {
            assert( index < SOURCE_COUNT );

            if ( index < SOURCE_COUNT )
            {
                this->mSourceBase[index] = gen;
                this->mSource[index] = dynamic_cast<Generator_FS<FS>*>( gen.get() );
            }
        }

    protected:
        std::array<Generator_FS<FS>*, SOURCE_COUNT> mSource;
    };
}

//template<typename F, FastSIMD::eLevel S>
//template<typename... P>
//typename FS_CLASS( FastNoise::Generator )<F, S>::int32v
//FS_CLASS( FastNoise::Generator )<F, S>::HashPrimesHB( int32v seed, P... primedPos )
//{
//    int32v hash = seed;
//    std::initializer_list<int32v>{ (hash ^= primedPos)... };
//
//    hash = hash * hash * hash * int32v( 60493 );
//    return hash;
//}
//
//template<typename F, FastSIMD::eLevel S>
//template<typename... P>
//typename FS_CLASS( FastNoise::Generator )<F, S>::float32v
//FS_CLASS( FastNoise::Generator )<F, S>::GetValueCoord( int32v seed, P... primedPos )
//{
//    int32v hash = seed;
//    std::initializer_list<int32v>{ (hash ^= primedPos)... };
//
//    hash = hash * hash * hash * int32v( 60493 );
//    return FS_Converti32_f32( hash ) * float32v( 1.f / INT_MAX );
//}
//
//template<typename F, FastSIMD::eLevel S>
//typename FS_CLASS( FastNoise::Generator )<F, S>::float32v
//FS_CLASS( FastNoise::Generator )<F, S>::InterpQuintic( float32v t )
//{
//    return t * t * t * FS_FMulAdd_f32(t, FS_FMulAdd_f32(t, float32v( 6 ), float32v( -15 )), float32v( 10 ));
//}
//
//template<typename F, FastSIMD::eLevel S>
//typename FS_CLASS( FastNoise::Generator )<F, S>::float32v
//FS_CLASS( FastNoise::Generator )<F, S>::Lerp( float32v a, float32v b, float32v t )
//{
//    return FS_FMulAdd_f32(t, b - a, a);
//}
//
//template<typename F, FastSIMD::eLevel S>
//void FS_CLASS( FastNoise::Modifier )<F, S>::SetSource( const std::shared_ptr<FastNoise::Generator>& gen )
//{
//    mSource = this->GetSIMD_Generator( gen.get() );
//
//    if ( mSource )
//    {
//        mSourceBase = gen;
//    }
//}