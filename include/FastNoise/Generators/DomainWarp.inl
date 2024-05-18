#include "DomainWarp.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainWarp, SIMD> : public virtual FastNoise::DomainWarp, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        Warp( seed, this->GetSourceValue( mWarpAmplitude, seed, pos... ), ( pos * float32v( this->mFrequency ) )..., pos... );

        return this->GetSourceValue( mSource, seed, pos...);
    }

public:
    float GetWarpFrequency() const { return this->mFrequency; }
    const FastNoise::HybridSource& GetWarpAmplitude() const { return mWarpAmplitude; }
    const FastNoise::GeneratorSource& GetWarpSource() const { return mSource; }

    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const = 0;
    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const = 0;
    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const = 0;
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainWarpGradient, SIMD> final : public virtual FastNoise::DomainWarpGradient, public FastSIMD::DispatchClass<FastNoise::DomainWarp, SIMD>
{
public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        float32v xs1 = InterpHermite( x - xs );
        float32v ys1 = InterpHermite( y - ys );
        float32v xs0 = float32v( 1 ) - xs1;
        float32v ys0 = float32v( 1 ) - ys1;

        float32v normalise( 1.0f / (0xffff / 2.0f) );

    #define GRADIENT_COORD( _x, _y )\
        int32v hash##_x##_y = HashPrimesHB(seed, x##_x, y##_y );\
        float32v contrib##_x##_y = normalise * xs##_x * ys##_y;\
        xWarp = FS::FMulAdd( contrib##_x##_y, FS::Convert<float>( hash##_x##_y & int32v( 0xffff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y, 16) ), yWarp )

        int32v hash00 = HashPrimesHB(seed, x0, y0 );
        float32v contrib00 = normalise * xs0 * ys0;
        float32v xWarp = contrib00 * FS::Convert<float>( hash00 & int32v( 0xffff ) );
        float32v yWarp = contrib00 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hash00, 16) );

        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        xWarp -= float32v( 1 );
        yWarp -= float32v( 1 );

        xOut = FS::FMulAdd( xWarp, warpAmp, xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp, yOut );

        float32v warpLengthSq = FS::FMulAdd( xWarp, xWarp, yWarp * yWarp );

        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v z0 = FS::Convert<int32_t>( zs ) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        float32v xs1 = InterpHermite( x - xs );
        float32v ys1 = InterpHermite( y - ys );
        float32v zs1 = InterpHermite( z - zs );
        float32v xs0 = float32v( 1 ) - xs1;
        float32v ys0 = float32v( 1 ) - ys1;
        float32v zs0 = float32v( 1 ) - zs1;

        float32v normalise( 1.0f / (0x3ff / 2.0f) );

    #define GRADIENT_COORD( _x, _y, _z )\
        int32v hash##_x##_y##_z = HashPrimesHB( seed, x##_x, y##_y, z##_z );\
        float32v contrib##_x##_y##_z = normalise * xs##_x * ys##_y * zs##_z;\
        xWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( hash##_x##_y##_z & int32v( 0x3ff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( (hash##_x##_y##_z >> 11) & int32v( 0x3ff ) ), yWarp );\
        zWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z, 22 ) ), zWarp )

        int32v hash000 = HashPrimesHB( seed, x0, y0, z0 );
        float32v contrib000 = normalise * xs0 * ys0 * zs0;
        float32v xWarp = contrib000 * FS::Convert<float>( hash000 & int32v( 0x3ff ) );
        float32v yWarp = contrib000 * FS::Convert<float>( (hash000 >> 11) & int32v( 0x3ff ) );
        float32v zWarp = contrib000 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hash000, 22 ) );

        GRADIENT_COORD( 1, 0, 0 );
        GRADIENT_COORD( 0, 1, 0 );
        GRADIENT_COORD( 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 1 );
        GRADIENT_COORD( 0, 1, 1 );
        GRADIENT_COORD( 1, 1, 1 );

    #undef GRADIENT_COORD

        xWarp -= float32v( 1 );
        yWarp -= float32v( 1 );
        zWarp -= float32v( 1 );

        xOut = FS::FMulAdd( xWarp, warpAmp, xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp, yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp, zOut );

        float32v warpLengthSq = FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, zWarp * zWarp ) );

        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );
        float32v ws = FS::Floor( w );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v z0 = FS::Convert<int32_t>( zs ) * int32v( Primes::Z );
        int32v w0 = FS::Convert<int32_t>( ws ) * int32v( Primes::W );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );
        int32v w1 = w0 + int32v( Primes::W );

        float32v xs1 = InterpHermite( x - xs );
        float32v ys1 = InterpHermite( y - ys );
        float32v zs1 = InterpHermite( z - zs );
        float32v ws1 = InterpHermite( w - ws );
        float32v xs0 = float32v( 1 ) - xs1;
        float32v ys0 = float32v( 1 ) - ys1;
        float32v zs0 = float32v( 1 ) - zs1;
        float32v ws0 = float32v( 1 ) - ws1;

        float32v normalise( 1.0f / (0xff / 2.0f) );

    #define GRADIENT_COORD( _x, _y, _z, _w )\
        int32v hash##_x##_y##_z##_w = HashPrimesHB( seed, x##_x, y##_y, z##_z, w##_w );\
        float32v contrib##_x##_y##_z##_w = normalise * xs##_x * ys##_y * zs##_z * ws##_w;\
        xWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( hash##_x##_y##_z##_w & int32v( 0xff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 8) & int32v( 0xff ) ), yWarp );\
        zWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 16) & int32v( 0xff ) ), zWarp );\
        wWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z##_w, 24 ) ), wWarp )

        int32v hash0000 = HashPrimesHB( seed, x0, y0, z0, w0 );
        float32v contrib0000 = normalise * xs0 * ys0 * zs0 * ws0;
        float32v xWarp = contrib0000 * FS::Convert<float>( hash0000 & int32v( 0xff ) );
        float32v yWarp = contrib0000 * FS::Convert<float>( (hash0000 >> 8) & int32v( 0xff ) );
        float32v zWarp = contrib0000 * FS::Convert<float>( (hash0000 >> 16) & int32v( 0xff ) );
        float32v wWarp = contrib0000 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hash0000, 24 ) );

        GRADIENT_COORD( 1, 0, 0, 0 );
        GRADIENT_COORD( 0, 1, 0, 0 );
        GRADIENT_COORD( 1, 1, 0, 0 );
        GRADIENT_COORD( 0, 0, 1, 0 );
        GRADIENT_COORD( 1, 0, 1, 0 );
        GRADIENT_COORD( 0, 1, 1, 0 );
        GRADIENT_COORD( 1, 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 0, 1 );
        GRADIENT_COORD( 0, 1, 0, 1 );
        GRADIENT_COORD( 1, 1, 0, 1 );
        GRADIENT_COORD( 0, 0, 1, 1 );
        GRADIENT_COORD( 1, 0, 1, 1 );
        GRADIENT_COORD( 0, 1, 1, 1 );
        GRADIENT_COORD( 1, 1, 1, 1 );

    #undef GRADIENT_COORD

        xWarp -= float32v( 1 );
        yWarp -= float32v( 1 );
        zWarp -= float32v( 1 );
        wWarp -= float32v( 1 );

        xOut = FS::FMulAdd( xWarp, warpAmp, xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp, yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp, zOut );
        wOut = FS::FMulAdd( wWarp, warpAmp, wOut );

        float32v warpLengthSq = FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, FS::FMulAdd( zWarp, zWarp, wWarp * wWarp ) ) );

        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
};

