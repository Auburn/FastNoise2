#include <cfloat>
#include <array>

#include "Cellular.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Cellular, SIMD> : public virtual FastNoise::Cellular, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
protected:
    const float kJitter2D = 0.437016f;
    const float kJitter3D = 0.396144f;
    const float kJitter4D = 0.366025f;
    const float kJitterIdx23 = 0.190983f;
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::CellularValue, SIMD> final : public virtual FastNoise::CellularValue, public FastSIMD::DispatchClass<FastNoise::Cellular, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS::Convert<float>( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcf );
                yd = FS::FMulAdd( yd, invMag, ycf );

                float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS::Convert<float>( hash );
                float32v newDistance = CalcDistance( mDistanceFunction, xd, yd );

                for( int i = 0; ; i++ )
                {
                    mask32v closer = newDistance < distance[i];

                    float32v localDistance = distance[i];
                    float32v localCellValue = value[i];

                    distance[i] = FS::Select( closer, newDistance, distance[i] );
                    value[i] = FS::Select( closer, newCellValue, value[i] );

                    if( i > mValueIndex )
                    {
                        break;
                    }

                    newDistance = FS::Select( closer, localDistance, newDistance );
                    newCellValue = FS::Select( closer, localCellValue, newCellValue );
                }

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return value[mValueIndex];
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );
        
        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        
        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
    
        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( ( hash >> 10 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( ( hash >> 20 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                
                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    xd = FS::FMulAdd( xd, invMag, xcf );
                    yd = FS::FMulAdd( yd, invMag, ycf );
                    zd = FS::FMulAdd( zd, invMag, zcf );
                
                    float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS::Convert<float>( hash );
                    float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd );
                
                    for( int i = 0; ; i++ )
                    {
                        mask32v closer = newDistance < distance[i];

                        float32v localDistance = distance[i];
                        float32v localCellValue = value[i];

                        distance[i] = FS::Select( closer, newDistance, distance[i] );
                        value[i] = FS::Select( closer, newCellValue, value[i] );

                        if( i > mValueIndex )
                        {
                            break;
                        }

                        newDistance = FS::Select( closer, localDistance, newDistance );
                        newCellValue = FS::Select( closer, localCellValue, newCellValue );
                    }
            
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }
    
        return value[mValueIndex];
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z , float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );
        
        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );
        
        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
        float32v wcfBase = FS::Convert<float>( wcBase ) - w;
    
        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        xd = FS::FMulAdd( xd, invMag, xcf );
                        yd = FS::FMulAdd( yd, invMag, ycf );
                        zd = FS::FMulAdd( zd, invMag, zcf );
                        wd = FS::FMulAdd( wd, invMag, wcf );

                        float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS::Convert<float>( hash );
                        float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        for( int i = 0; ; i++ )
                        {
                            mask32v closer = newDistance < distance[i];

                            float32v localDistance = distance[i];
                            float32v localCellValue = value[i];

                            distance[i] = FS::Select( closer, newDistance, distance[i] );
                            value[i] = FS::Select( closer, newCellValue, value[i] );

                            if( i > mValueIndex )
                            {
                                break;
                            }

                            newDistance = FS::Select( closer, localDistance, newDistance );
                            newCellValue = FS::Select( closer, localCellValue, newCellValue );
                        }

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }
    
        return value[mValueIndex];
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::CellularDistance, SIMD> final : public virtual FastNoise::CellularDistance, public FastSIMD::DispatchClass<FastNoise::Cellular, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS::Convert<float>( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcf );
                yd = FS::FMulAdd( yd, invMag, ycf );

                float32v newDistance = CalcDistance( mDistanceFunction, xd, yd );

                for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                {
                    distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                }

                distance[0] = FS::Min( distance[0], newDistance );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    xd = FS::FMulAdd( xd, invMag, xcf );
                    yd = FS::FMulAdd( yd, invMag, ycf );
                    zd = FS::FMulAdd( zd, invMag, zcf );

                    float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd );

                    for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                    {
                        distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                    }

                    distance[0] = FS::Min( distance[0], newDistance );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
        float32v wcfBase = FS::Convert<float>( wcBase ) - w;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        xd = FS::FMulAdd( xd, invMag, xcf );
                        yd = FS::FMulAdd( yd, invMag, ycf );
                        zd = FS::FMulAdd( zd, invMag, zcf );
                        wd = FS::FMulAdd( wd, invMag, wcf );

                        float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                        {
                            distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                        }

                        distance[0] = FS::Min( distance[0], newDistance );

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance );
    }

    FS_FORCEINLINE float32v GetReturn( std::array<float32v, kMaxDistanceCount>& distance ) const
    {
        if( mDistanceFunction == FastNoise::DistanceFunction::Euclidean )
        {
            distance[mDistanceIndex0] *= FS::InvSqrt( distance[mDistanceIndex0] );
            distance[mDistanceIndex1] *= FS::InvSqrt( distance[mDistanceIndex1] );
        }

        switch( mReturnType )
        {
        default:
        case ReturnType::Index0:
        {
            return distance[mDistanceIndex0];
        }
        case ReturnType::Index0Add1:
        {
            return distance[mDistanceIndex0] + distance[mDistanceIndex1];
        }
        case ReturnType::Index0Sub1:
        {
            return distance[mDistanceIndex0] - distance[mDistanceIndex1];
        }
        case ReturnType::Index0Mul1:
        {
            return distance[mDistanceIndex0] * distance[mDistanceIndex1];
        }
        case ReturnType::Index0Div1:
        {
            return distance[mDistanceIndex0] * FS::Reciprocal( distance[mDistanceIndex1] );
        }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::CellularLookup, SIMD> final : public virtual FastNoise::CellularLookup, public FastSIMD::DispatchClass<FastNoise::Cellular, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );
        float32v distance( FLT_MAX );
        float32v cellX, cellY;

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS::Convert<float>( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcf );
                yd = FS::FMulAdd( yd, invMag, ycf );

                float32v newDistance = CalcDistance( mDistanceFunction, xd, yd );

                mask32v closer = newDistance < distance;
                distance = FS::Min( newDistance, distance );

                cellX = FS::Select( closer, xd + x, cellX );
                cellY = FS::Select( closer, yd + y, cellY );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );
        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ;

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    xd = FS::FMulAdd( xd, invMag, xcf );
                    yd = FS::FMulAdd( yd, invMag, ycf );
                    zd = FS::FMulAdd( zd, invMag, zcf );

                    float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd );

                    mask32v closer = newDistance < distance;
                    distance = FS::Min( newDistance, distance );

                    cellX = FS::Select( closer, xd + x, cellX );
                    cellY = FS::Select( closer, yd + y, cellY );
                    cellZ = FS::Select( closer, zd + z, cellZ );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ), cellZ * float32v( mLookupFreq ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );
        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ, cellW;

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
        float32v wcfBase = FS::Convert<float>( wcBase ) - w;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        xd = FS::FMulAdd( xd, invMag, xcf );
                        yd = FS::FMulAdd( yd, invMag, ycf );
                        zd = FS::FMulAdd( zd, invMag, zcf );
                        wd = FS::FMulAdd( wd, invMag, wcf );

                        float32v newDistance = CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        mask32v closer = newDistance < distance;
                        distance = FS::Min( newDistance, distance );

                        cellX = FS::Select( closer, xd + x, cellX );
                        cellY = FS::Select( closer, yd + y, cellY );
                        cellZ = FS::Select( closer, zd + z, cellZ );
                        cellW = FS::Select( closer, wd + w, cellW );

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ), cellZ * float32v( mLookupFreq ), cellW * float32v( mLookupFreq ) );
    }
};
