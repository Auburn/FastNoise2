#include "FastSIMD/InlInclude.h"

#include "Cellular.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::Cellular, FS> : public virtual FastNoise::Cellular, public FS_T<FastNoise::Generator, FS>
{
protected:
    template<typename... P>
    FS_INLINE float32v GetDistance( float32v dX, P... d )
    {
        switch ( mDistanceFunction )
        {
        default:
        case DistanceFunction::Euclidean:
        {
            float32v distSqr = dX * dX;
            (void( distSqr = FS_FMulAdd_f32( d, d, distSqr ) ), ...);

            return FS_InvSqrt_f32( distSqr ) * distSqr;
        }
        case DistanceFunction::EuclideanSquared:
        {
            float32v distSqr = dX * dX;
            (void( distSqr = FS_FMulAdd_f32( d, d, distSqr ) ), ...);

            return distSqr;
        }

        case DistanceFunction::Manhattan:
        {
            float32v dist = FS_Abs_f32( dX );
            dist += (FS_Abs_f32( d ) + ...);

            return dist;
        }

        case DistanceFunction::Natural:
        {
            float32v both = FS_FMulAdd_f32( dX, dX, FS_Abs_f32( dX ) );
            (void( both += FS_FMulAdd_f32( d, d, FS_Abs_f32( d ) )), ...);

            return both;
        }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::CellularValue, FS> : public virtual FastNoise::CellularValue, public FS_T<FastNoise::Cellular, FS>
{
public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) final
    {
        float32v jitter = float32v( kJitter2D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );
        float32v cellValue;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newCellValue = float32v( 1.0f / INT_MAX ) * FS_Converti32_f32( hash );
                float32v newDistance = this->GetDistance( xd, yd );

                mask32v closer = FS_LessThan_f32( newDistance, distance );

                distance = FS_Min_f32( newDistance, distance );
                cellValue = FS_Select_f32( closer, newCellValue, cellValue );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return cellValue;
    }

    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) final
    {
        float32v jitter = float32v( kJitter3D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );
        float32v cellValue;
        
        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );
        
        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;
    
        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
    
        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for ( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( ( hash >> 10 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( ( hash >> 20 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                
                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );
                
                    float32v newCellValue = float32v( 1.0f / INT_MAX ) * FS_Converti32_f32( hash );
                    float32v newDistance = this->GetDistance( xd, yd, zd );
				
				    mask32v closer = FS_LessThan_f32( newDistance, distance );
				
				    distance = FS_Min_f32( newDistance, distance );
				    cellValue = FS_Select_f32( closer, newCellValue, cellValue );
			
			        zcf += float32v( 1 );
			        zc += int32v( Primes::Z );
		        }
		        ycf += float32v( 1 );
		        yc += int32v( Primes::Y );
	        }
	        xcf += float32v( 1 );
	        xc += int32v( Primes::X );
	    }
	
        return cellValue;
    }
};

template<typename FS>
class FS_T<FastNoise::CellularDistance, FS> : public virtual FastNoise::CellularDistance, public FS_T<FastNoise::Cellular, FS>
{
public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) final
    {
        float32v jitter = float32v( kJitter2D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newDistance = this->GetDistance( xd, yd );

                distance = FS_Min_f32( newDistance, distance );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        if ( mDistanceFunction == DistanceFunction::Euclidean )
        {
            distance *= FS_InvSqrt_f32( distance );
        }

        return distance;
    }

    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) final
    {
        float32v jitter = float32v( kJitter3D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for ( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );

                    float32v newDistance = this->GetDistance( xd, yd, zd );

                    distance = FS_Min_f32( newDistance, distance );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        if ( mDistanceFunction == DistanceFunction::Euclidean )
        {
            distance *= FS_InvSqrt_f32( distance );
        }

        return distance;
    }
};

template<typename FS>
class FS_T<FastNoise::CellularLookup, FS> : public virtual FastNoise::CellularLookup, public FS_T<FastNoise::Cellular, FS>
{
public:
    virtual void SetLookup( const std::shared_ptr<FastNoise::Generator>& gen ) final
    {
        assert( gen->GetSIMDLevel() == GetSIMDLevel() );

        if ( gen->GetSIMDLevel() == GetSIMDLevel() )
        {
            this->mLookupBase = gen;
            this->mLookup = dynamic_cast<FS_T<FastNoise::Generator, FS>*>( gen.get() );
        }
    }

    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) final
    {
        float32v jitter = float32v( kJitter2D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );
        float32v cellX, cellY;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newDistance = this->GetDistance( xd, yd );

                mask32v closer = FS_LessThan_f32( newDistance, distance );
                distance = FS_Min_f32( newDistance, distance );

                cellX = FS_Select_f32( closer, xd + x, cellX );
                cellY = FS_Select_f32( closer, yd + y, cellY );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return mLookup->Gen( seed - int32v( -1 ), cellX * float32v( mLookupFreqX ), cellY * float32v( mLookupFreqY ) );
    }

    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) final
    {
        float32v jitter = float32v( kJitter3D * mJitterModifier );
        float32v distance = float32v( FLT_MAX );
        float32v cellX, cellY, cellZ;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for ( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for ( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );

                    float32v newDistance = this->GetDistance( xd, yd, zd );

                    mask32v closer = FS_LessThan_f32( newDistance, distance );
                    distance = FS_Min_f32( newDistance, distance );

                    cellX = FS_Select_f32( closer, xd + x, cellX );
                    cellY = FS_Select_f32( closer, yd + y, cellY );
                    cellZ = FS_Select_f32( closer, zd + z, cellZ );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return mLookup->Gen( seed - int32v( -1 ), cellX * float32v( mLookupFreqX ), cellY * float32v( mLookupFreqY ), cellZ * float32v( mLookupFreqZ ) );
    }

protected:
    FS_T<Generator, FS>* mLookup;
};
