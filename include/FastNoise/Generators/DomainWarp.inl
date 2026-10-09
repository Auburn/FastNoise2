#include "DomainWarp.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarp, SIMD> : public virtual DomainWarp, public DispatchClass<Seeded<ScalableGenerator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        // Seed offset is applied inside Warp() so it also applies when called from DomainWarpFractal
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
class FastSIMD::DispatchClass<DomainWarpGradient, SIMD> final : public virtual DomainWarpGradient, public DispatchClass<DomainWarp, SIMD>
{
public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        seed += int32v( mSeedOffset );
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
        float32v contrib##_x##_y = xs##_x * ys##_y;\
        xWarp = FS::FMulAdd( contrib##_x##_y, FS::Convert<float>( hash##_x##_y & int32v( 0xffff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y, 16 ) ), yWarp )

        int32v hash00 = HashPrimesHB(seed, x0, y0 );
        float32v contrib00 = xs0 * ys0;
        float32v xWarp = contrib00 * FS::Convert<float>( hash00 & int32v( 0xffff ) );
        float32v yWarp = contrib00 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hash00, 16 ) );

        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        xWarp = FS::FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FS::FMulSub( yWarp, normalise, float32v( 1 ) );

        xOut = FS::FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );

        return FS::FMulAdd( xWarp, xWarp, yWarp * yWarp );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        seed += int32v( mSeedOffset );
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
        float32v contrib##_x##_y##_z = xs##_x * ys##_y * zs##_z;\
        xWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( hash##_x##_y##_z & int32v( 0x3ff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( (hash##_x##_y##_z >> 11) & int32v( 0x3ff ) ), yWarp );\
        zWarp = FS::FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z, 22 ) ), zWarp )

        int32v hash000 = HashPrimesHB( seed, x0, y0, z0 );
        float32v contrib000 = xs0 * ys0 * zs0;
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

        xWarp = FS::FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FS::FMulSub( yWarp, normalise, float32v( 1 ) );
        zWarp = FS::FMulSub( zWarp, normalise, float32v( 1 ) );

        xOut = FS::FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp * float32v( this->mAxisScale[2] ), zOut );

        return FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, zWarp * zWarp ) );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        seed += int32v( mSeedOffset );

        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX512_F ) || !( SIMD & FastSIMD::FeatureFlag::AVX2 ) )
        {
            return WarpImpl( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
        else // Process each native register half separately, too many live values to fit in registers otherwise
        {
            auto value0 = WarpImpl( seed.v0, warpAmp.v0, x.v0, y.v0, z.v0, w.v0, xOut.v0, yOut.v0, zOut.v0, wOut.v0 );
            auto value1 = WarpImpl( seed.v1, warpAmp.v1, x.v1, y.v1, z.v1, w.v1, xOut.v1, yOut.v1, zOut.v1, wOut.v1 );
            return float32v( value0, value1 );
        }
    }

private:
    // Width generic copies of InterpHermite/HashPrimesHB from Utils.inl, used on native register halves
    template<typename F>
    FS_FORCEINLINE static F InterpHermiteHalf( F t )
    {
        return t * t * FS::FNMulAdd( t, F( 2 ), F( 3 ) );
    }

    template<typename I, typename... P>
    FS_FORCEINLINE static I HashPrimesHBHalf( I seed, P... primedPos )
    {
        I hash = seed;
        hash ^= ( primedPos ^ ... );

        hash *= I( HashMultiplier::A );
        return hash;
    }

    template<typename I, typename F>
    FS_FORCEINLINE F WarpImpl( I seed, F warpAmp, F x, F y, F z, F w, F& xOut, F& yOut, F& zOut, F& wOut ) const
    {
        F xs = FS::Floor( x );
        F ys = FS::Floor( y );
        F zs = FS::Floor( z );
        F ws = FS::Floor( w );

        I x0 = FS::Convert<int32_t>( xs ) * I( Primes::X );
        I y0 = FS::Convert<int32_t>( ys ) * I( Primes::Y );
        I z0 = FS::Convert<int32_t>( zs ) * I( Primes::Z );
        I w0 = FS::Convert<int32_t>( ws ) * I( Primes::W );
        I x1 = x0 + I( Primes::X );
        I y1 = y0 + I( Primes::Y );
        I z1 = z0 + I( Primes::Z );
        I w1 = w0 + I( Primes::W );

        F xs1 = InterpHermiteHalf( x - xs );
        F ys1 = InterpHermiteHalf( y - ys );
        F zs1 = InterpHermiteHalf( z - zs );
        F ws1 = InterpHermiteHalf( w - ws );
        F xs0 = F( 1 ) - xs1;
        F ys0 = F( 1 ) - ys1;
        F zs0 = F( 1 ) - zs1;
        F ws0 = F( 1 ) - ws1;

        F normalise( 1.0f / (0xff / 2.0f) );

    #define GRADIENT_COORD( _x, _y, _z, _w )\
        I hash##_x##_y##_z##_w = HashPrimesHBHalf( seed, x##_x, y##_y, z##_z, w##_w );\
        F contrib##_x##_y##_z##_w = xs##_x * ys##_y * zs##_z * ws##_w;\
        xWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( hash##_x##_y##_z##_w & I( 0xff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 8) & I( 0xff ) ), yWarp );\
        zWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 16) & I( 0xff ) ), zWarp );\
        wWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z##_w, 24 ) ), wWarp )

        I hash0000 = HashPrimesHBHalf( seed, x0, y0, z0, w0 );
        F contrib0000 = xs0 * ys0 * zs0 * ws0;
        F xWarp = contrib0000 * FS::Convert<float>( hash0000 & I( 0xff ) );
        F yWarp = contrib0000 * FS::Convert<float>( (hash0000 >> 8) & I( 0xff ) );
        F zWarp = contrib0000 * FS::Convert<float>( (hash0000 >> 16) & I( 0xff ) );
        F wWarp = contrib0000 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hash0000, 24 ) );

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

        xWarp = FS::FMulSub( xWarp, normalise, F( 1 ) );
        yWarp = FS::FMulSub( yWarp, normalise, F( 1 ) );
        zWarp = FS::FMulSub( zWarp, normalise, F( 1 ) );
        wWarp = FS::FMulSub( wWarp, normalise, F( 1 ) );

        xOut = FS::FMulAdd( xWarp, warpAmp * F( this->mAxisScale[0] ), xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp * F( this->mAxisScale[1] ), yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp * F( this->mAxisScale[2] ), zOut );
        wOut = FS::FMulAdd( wWarp, warpAmp * F( this->mAxisScale[3] ), wOut );

        return FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, FS::FMulAdd( zWarp, zWarp, wWarp * wWarp ) ) );
    }
};

