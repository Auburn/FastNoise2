#pragma once
#include <climits>

namespace FastNoise
{    
    namespace Primes
    {
        static constexpr int X = 501125321;
        static constexpr int Y = 1136930381;
        static constexpr int Z = 1720413743;
        static constexpr int W = 1066037191;

        static constexpr int Lookup[] = { X,Y,Z,W };
    }

    static constexpr float ROOT2 = 1.4142135623730950488f;
    static constexpr float ROOT3 = 1.7320508075688772935f;

    template<FastSIMD::FeatureSet SIMD = FASTSIMD_DEFAULT_FEATURE_SET>
    FS_FORCEINLINE static float32v GetGradientDotFancy( int32v hash, float32v fX, float32v fY )
    {
        int32v index = FS::Convert<int32_t>( FS::Convert<float>( hash & int32v( 0x3FFFFF ) ) * float32v( 1.3333333333333333f ) );

        // Bit-3 = Choose X Y ordering
        mask32v bit3;
        
        if constexpr( (SIMD & FastSIMD::FeatureFlag::SSE41) && !(SIMD & FastSIMD::FeatureFlag::AVX512_F) )
        {
            bit3 = FS::Cast<FS::Mask<32>>( index << 29 );
        }      
        else
        {
            bit3 = ( index & int32v( 1 << 2 ) ) == int32v( 0 );
        }

        float32v a = FS::Select( bit3, fY, fX );
        float32v b = FS::Select( bit3, fX, fY );

        // Bit-1 = b flip sign
        b ^= FS::Cast<float>( index << 31 );

        // Bit-2 = Mul a by 2 or Root3     
        mask32v bit2 = ( index & int32v( 2 ) ) == int32v( 0 );   

        a *= FS::Select( bit2, float32v( 2 ), float32v( ROOT3 ) );
        // b zero value if a mul 2
        float32v c = FS::MaskedAdd( bit2, a, b );

        // Bit-4 = Flip sign of a + b
        return c ^ FS::Cast<float>( (index >> 3) << 31 );
    }

    //template<typename SIMD = FS, std::enable_if_t<SIMD::SIMD_Level == FastSIMD::Level_AVX2>* = nullptr>
    //FS_FORCEINLINE static float32v GetGradientDotFancy( int32v hash, float32v fX, float32v fY )
    //{
    //    int32v index = FS::Convert<int32_t>( FS::Convert<float>( hash & int32v( 0x3FFFFF ) ) * float32v( 1.3333333333333333f ) );

    //    float32v gX = _mm256_permutevar8x32_ps( float32v( ROOT3, ROOT3, 2, 2, 1, -1, 0, 0 ), index );
    //    float32v gY = _mm256_permutevar8x32_ps( float32v( 1, -1, 0, 0, ROOT3, ROOT3, 2, 2 ), index );

    //    // Bit-8 = Flip sign of a + b
    //    return FS::FMulAdd( gX, fX, fY * gY ) ^ FS::Cast<float>( (index >> 3) << 31 );
    //}

    //template<typename SIMD = FS, std::enable_if_t<(SIMD::SIMD_Level == FastSIMD::Level_AVX512)>* = nullptr>
    //FS_FORCEINLINE static float32v GetGradientDotFancy( int32v hash, float32v fX, float32v fY )
    //{
    //    int32v index = FS::Convert<int32_t>( FS::Convert<float>( hash & int32v( 0x3FFFFF ) ) * float32v( 1.3333333333333333f ) );

    //    float32v gX = _mm512_permutexvar_ps( index, float32v( ROOT3, ROOT3, 2, 2, 1, -1, 0, 0, -ROOT3, -ROOT3, -2, -2, -1, 1, 0, 0 ) );
    //    float32v gY = _mm512_permutexvar_ps( index, float32v( 1, -1, 0, 0, ROOT3, ROOT3, 2, 2, -1, 1, 0, 0, -ROOT3, -ROOT3, -2, -2 ) );

    //    return FS::FMulAdd( gX, fX, fY * gY );
    //}


    template<FastSIMD::FeatureSet SIMD = FASTSIMD_DEFAULT_FEATURE_SET>
    FS_FORCEINLINE static float32v GetGradientDot( int32v hash, float32v fX, float32v fY )
    {
        // ( 1+R2, 1 ) ( -1-R2, 1 ) ( 1+R2, -1 ) ( -1-R2, -1 )
        // ( 1, 1+R2 ) ( 1, -1-R2 ) ( -1, 1+R2 ) ( -1, -1-R2 )

        /*if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = _mm512_permutexvar_ps( hash, float32v( 1 + ROOT2, -1 - ROOT2, 1 + ROOT2, -1 - ROOT2, 1, -1, 1, -1, 1 + ROOT2, -1 - ROOT2, 1 + ROOT2, -1 - ROOT2, 1, -1, 1, -1 ) );
            float32v gY = _mm512_permutexvar_ps( hash, float32v( 1, 1, -1, -1, 1 + ROOT2, 1 + ROOT2, -1 - ROOT2, -1 - ROOT2, 1, 1, -1, -1, 1 + ROOT2, 1 + ROOT2, -1 - ROOT2, -1 - ROOT2 ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v gX = _mm256_permutevar8x32_ps( float32v( 1 + ROOT2, -1 - ROOT2, 1 + ROOT2, -1 - ROOT2, 1, -1, 1, -1 ), hash );
            float32v gY = _mm256_permutevar8x32_ps( float32v( 1, 1, -1, -1, 1 + ROOT2, 1 + ROOT2, -1 - ROOT2, -1 - ROOT2 ), hash );

            return FS::FMulAdd( gX, fX, fY * gY );
        }*/

        int32v bit1 = hash << 31;
        int32v bit2 = (hash >> 1) << 31;
        int32v bit4 = hash << 29;

        if constexpr( !( SIMD & FastSIMD::FeatureFlag::SSE41 ) )
        {
            bit4 >>= 31;
        }

        auto bit4Mask = FS::Cast<FS::Mask<32, false>>( bit4 );

        fX ^= FS::Cast<float>( bit1 );
        fY ^= FS::Cast<float>( bit2 );
        
        float32v a = FS::Select( bit4Mask, fY, fX );
        float32v b = FS::Select( bit4Mask, fX, fY );
        
        return FS::FMulAdd( float32v( 1.0f + ROOT2 ), a, b );
    }
    
    template<FastSIMD::FeatureSet SIMD = FASTSIMD_DEFAULT_FEATURE_SET>
    FS_FORCEINLINE static float32v GetGradientDot( int32v hash, float32v fX, float32v fY, float32v fZ )
    {        
        /*if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = _mm512_permutexvar_ps( hash, float32v( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 ) );
            float32v gY = _mm512_permutexvar_ps( hash, float32v( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            float32v gZ = _mm512_permutexvar_ps( hash, float32v( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, fZ * gZ ));
        }*/

        int32v hasha13 = hash & int32v( 13 );

        //if h < 8 then x, else y
        mask32v less8;
        if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
        {
            less8 = FS::Cast<FS::Mask<32>>( hasha13 << 28 );
        }
        else
        {
            less8 = hasha13 < int32v( 8 );    
        }
        float32v u = FS::Select( less8, fX, fY );

        //if h < 4 then y else if h is 12 or 14 then x else z
        float32v v = FS::Select( hasha13 == int32v( 12 ), fX, fZ );
        v = FS::Select( hasha13 < int32v( 2 ), fY, v );

        //if h1 then -u else u
        //if h2 then -v else v
        float32v h1 = FS::Cast<float>( hash << 31 );
        float32v h2 = FS::Cast<float>( (hash >> 1) << 31 );
        //then add them
        return ( u ^ h1 ) + ( v ^ h2 );
    }
    
    template<FastSIMD::FeatureSet SIMD = FASTSIMD_DEFAULT_FEATURE_SET>
    FS_FORCEINLINE static float32v GetGradientDot( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW )
    {
        /*if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = _mm512_permutex2var_ps( float32v( 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ), hash, float32v( 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            float32v gY = _mm512_permutex2var_ps( float32v( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ), hash, float32v( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ) );
            float32v gZ = _mm512_permutex2var_ps( float32v( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ), hash, float32v( 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, -1, -1, -1, -1 ) );
            float32v gW = _mm512_permutex2var_ps( float32v( 1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1, 1, -1, -1, -1, -1 ), hash, float32v( 1, 1, 1, 1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, FS::FMulAdd( fZ, gZ, fW * gW ) ));
        }*/

        int32v p = hash & int32v( 3 << 3 );

        float32v a = FS::Select( p > int32v( 0 ), fX, fY );
        float32v b;
        if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
        {
            b = FS::Select( FS::Cast<FS::Mask<32>>( hash << 27 ), fY, fZ );
        }
        else
        {
            b = FS::Select( p > int32v( 1 << 3 ), fY, fZ );        
        }
        float32v c = FS::Select( p > int32v( 2 << 3 ), fZ, fW );

        float32v aSign = FS::Cast<float>( hash << 31 );
        float32v bSign = FS::Cast<float>( (hash >> 1) << 31 );
        float32v cSign = FS::Cast<float>( (hash >> 2) << 31 );

        return ( a ^ aSign ) + ( b ^ bSign ) + ( c ^ cSign );
    }

    template<typename... P>
    FS_FORCEINLINE static int32v HashPrimes( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);

        hash *= int32v( 0x27d4eb2d );
        return (hash >> 15) ^ hash;
    }

    template<typename... P>
    FS_FORCEINLINE static int32v HashPrimesHB( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);
        
        hash *= int32v( 0x27d4eb2d );
        return hash;
    }  

    template<typename... P>
     FS_FORCEINLINE static float32v GetValueCoord( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);
        
        hash *= hash * int32v( 0x27d4eb2d );
        return FS::Convert<float>( hash ) * float32v( 1.0f / (float)INT_MAX );
    }
     
    FS_FORCEINLINE static float32v Lerp( float32v a, float32v b, float32v t )
    {
        return FS::FMulAdd( t, b - a, a );
    }
    
    FS_FORCEINLINE static float32v InterpHermite( float32v t )
    {
        return t * t * FS::FNMulAdd( t, float32v( 2 ), float32v( 3 ));
    }
     
     FS_FORCEINLINE static float32v InterpQuintic( float32v t )
    {
        return t * t * t * FS::FMulAdd( t, FS::FMulAdd( t, float32v( 6 ), float32v( -15 )), float32v( 10 ) );
    }

    template<typename... P>
    FS_FORCEINLINE static float32v CalcDistance( DistanceFunction distFunc, float32v dX, P... d )
    {
        switch( distFunc )
        {
            default:
            case DistanceFunction::Euclidean:
            {
                float32v distSqr = dX * dX;
                ((distSqr = FS::FMulAdd( d, d, distSqr )), ...);

                return FS::InvSqrt( distSqr ) * distSqr;
            }

            case DistanceFunction::EuclideanSquared:
            {
                float32v distSqr = dX * dX;
                ((distSqr = FS::FMulAdd( d, d, distSqr )), ...);

                return distSqr;
            }

            case DistanceFunction::Manhattan:
            {
                float32v dist = FS::Abs( dX );
                dist += (FS::Abs( d ) + ...);

                return dist;
            }

            case DistanceFunction::Hybrid:
            {
                float32v both = FS::FMulAdd( dX, dX, FS::Abs( dX ) );
                ((both += FS::FMulAdd( d, d, FS::Abs( d ) )), ...);

                return both;
            }

            case DistanceFunction::MaxAxis:
            {
                float32v max = FS::Abs( dX );
                ((max = FS::Max( FS::Abs(d), max )), ...);

                return max;
            }
        }
    }    
}