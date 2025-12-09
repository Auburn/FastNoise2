#include "Perlin.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Perlin, SIMD> final : public virtual Perlin, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        float32v xf0 = xs = x - xs;
        float32v yf0 = ys = y - ys;
        float32v xf1 = xf0 - float32v( 1 );
        float32v yf1 = yf0 - float32v( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );

        float32v value = Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0 ), xf0, yf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0 ), xf1, yf0 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1 ), xf0, yf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1 ), xf1, yf1 ), xs ), ys );

        constexpr float kBounding = 1.726796627044677734375f;

        return this->ScaleOutput( value, -kBounding, kBounding );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y, z );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v z0 = FS::Convert<int32_t>( zs ) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        float32v xf0 = xs = x - xs;
        float32v yf0 = ys = y - ys;
        float32v zf0 = zs = z - zs;
        float32v xf1 = xf0 - float32v( 1 );
        float32v yf1 = yf0 - float32v( 1 );
        float32v zf1 = zf0 - float32v( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );
        zs = InterpQuintic( zs );

        float32v value = Lerp( Lerp(
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y0, z0 ), xf0, yf0, zf0 ), GetGradientDotCommon( HashPrimes( seed, x1, y0, z0 ), xf1, yf0, zf0 ), xs ),
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y1, z0 ), xf0, yf1, zf0 ), GetGradientDotCommon( HashPrimes( seed, x1, y1, z0 ), xf1, yf1, zf0 ), xs ), ys ),
            Lerp(
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y0, z1 ), xf0, yf0, zf1 ), GetGradientDotCommon( HashPrimes( seed, x1, y0, z1 ), xf1, yf0, zf1 ), xs ),
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y1, z1 ), xf0, yf1, zf1 ), GetGradientDotCommon( HashPrimes( seed, x1, y1, z1 ), xf1, yf1, zf1 ), xs ), ys ), zs );

        constexpr double kBounding = 1.0363423824310302734375;

        return this->ScaleOutput( value, -kBounding, kBounding );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y, z, w );

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

        float32v xf0 = xs = x - xs;
        float32v yf0 = ys = y - ys;
        float32v zf0 = zs = z - zs;
        float32v wf0 = ws = w - ws;
        float32v xf1 = xf0 - float32v( 1 );
        float32v yf1 = yf0 - float32v( 1 );
        float32v zf1 = zf0 - float32v( 1 );
        float32v wf1 = wf0 - float32v( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );
        zs = InterpQuintic( zs );
        ws = InterpQuintic( ws );

        float32v value = Lerp( Lerp( Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0, z0, w0 ), xf0, yf0, zf0, wf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0, z0, w0 ), xf1, yf0, zf0, wf0 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1, z0, w0 ), xf0, yf1, zf0, wf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1, z0, w0 ), xf1, yf1, zf0, wf0 ), xs ), ys ),
            Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0, z1, w0 ), xf0, yf0, zf1, wf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0, z1, w0 ), xf1, yf0, zf1, wf0 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1, z1, w0 ), xf0, yf1, zf1, wf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1, z1, w0 ), xf1, yf1, zf1, wf0 ), xs ), ys ), zs ),
            Lerp( Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0, z0, w1 ), xf0, yf0, zf0, wf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0, z0, w1 ), xf1, yf0, zf0, wf1 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1, z0, w1 ), xf0, yf1, zf0, wf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1, z0, w1 ), xf1, yf1, zf0, wf1 ), xs ), ys ),
            Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0, z1, w1 ), xf0, yf0, zf1, wf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0, z1, w1 ), xf1, yf0, zf1, wf1 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1, z1, w1 ), xf0, yf1, zf1, wf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1, z1, w1 ), xf1, yf1, zf1, wf1 ), xs ), ys ) , zs ), ws );

        constexpr float kBounding = 1.33858621120452880859375;

        return this->ScaleOutput( value, -kBounding, kBounding );
    }
};
