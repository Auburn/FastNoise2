#include "Perlin.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Perlin, SIMD> final : public virtual Perlin, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v x0 = FloorToInt( x, xs ) * int32v( Primes::X );
        int32v y0 = FloorToInt( y, ys ) * int32v( Primes::Y );
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

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );

        int32v xi = FloorToInt( x, xs );
        int32v yi = FloorToInt( y, ys );
        int32v zi = FloorToInt( z, zs );

        float32v xf0 = xs = x - xs;
        float32v yf0 = ys = y - ys;
        float32v zf0 = zs = z - zs;
        float32v xf1 = xf0 - float32v( 1 );
        float32v yf1 = yf0 - float32v( 1 );
        float32v zf1 = zf0 - float32v( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );
        zs = InterpQuintic( zs );

        constexpr double kBounding = 1.0363423824310302734375;

        // Corner-major path for lane-coherent vectors (uniform grids): hash the lattice block once, permute the hashes per lane
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, yi, zi ) )
            {
                auto seedB = LatticeBlockBroadcast( seed );
                auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
                auto yp = LatticeBlockPrimed<1>( yi, Primes::Y );
                auto hashZ0 = HashPrimes( seedB, xp, yp, LatticeBlockPrimed<2, 0>( zi, Primes::Z ) );
                auto hashZ1 = HashPrimes( seedB, xp, yp, LatticeBlockPrimed<2, 1>( zi, Primes::Z ) );
                int32v xOffset1 = xOffset + int32v( 1 );
                int32v xOffset4 = xOffset + int32v( 4 );
                int32v xOffset5 = xOffset + int32v( 5 );

                using NF = decltype( FS::Cast<float>( hashZ0 ) );
                NF one( 1 ), zero( 0 );
                NF gXZ0 = GetGradientDotCommon( hashZ0, one, zero, zero );
                NF gYZ0 = GetGradientDotCommon( hashZ0, zero, one, zero );
                NF gZZ0 = GetGradientDotCommon( hashZ0, zero, zero, one );
                NF gXZ1 = GetGradientDotCommon( hashZ1, one, zero, zero );
                NF gYZ1 = GetGradientDotCommon( hashZ1, zero, one, zero );
                NF gZZ1 = GetGradientDotCommon( hashZ1, zero, zero, one );

                auto dot = [&]( const NF& gX, const NF& gY, const NF& gZ, const int32v& idx, const float32v& fX, const float32v& fY, const float32v& fZ )
                {
                    return FS::FMulAdd( LatticeBlockPermute<float32v>( gX, idx ), fX, FS::FMulAdd( fY, LatticeBlockPermute<float32v>( gY, idx ), fZ * LatticeBlockPermute<float32v>( gZ, idx ) ) );
                };

                float32v value = Lerp( Lerp(
                    Lerp( dot( gXZ0, gYZ0, gZZ0, xOffset, xf0, yf0, zf0 ), dot( gXZ0, gYZ0, gZZ0, xOffset1, xf1, yf0, zf0 ), xs ),
                    Lerp( dot( gXZ0, gYZ0, gZZ0, xOffset4, xf0, yf1, zf0 ), dot( gXZ0, gYZ0, gZZ0, xOffset5, xf1, yf1, zf0 ), xs ), ys ),
                    Lerp(
                    Lerp( dot( gXZ1, gYZ1, gZZ1, xOffset, xf0, yf0, zf1 ), dot( gXZ1, gYZ1, gZZ1, xOffset1, xf1, yf0, zf1 ), xs ),
                    Lerp( dot( gXZ1, gYZ1, gZZ1, xOffset4, xf0, yf1, zf1 ), dot( gXZ1, gYZ1, gZZ1, xOffset5, xf1, yf1, zf1 ), xs ), ys ), zs );

                return this->ScaleOutput( this, value, -kBounding, kBounding );
            }
        }

        int32v x0 = xi * int32v( Primes::X );
        int32v y0 = yi * int32v( Primes::Y );
        int32v z0 = zi * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        float32v value = Lerp( Lerp(
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y0, z0 ), xf0, yf0, zf0 ), GetGradientDotCommon( HashPrimes( seed, x1, y0, z0 ), xf1, yf0, zf0 ), xs ),
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y1, z0 ), xf0, yf1, zf0 ), GetGradientDotCommon( HashPrimes( seed, x1, y1, z0 ), xf1, yf1, zf0 ), xs ), ys ),
            Lerp(
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y0, z1 ), xf0, yf0, zf1 ), GetGradientDotCommon( HashPrimes( seed, x1, y0, z1 ), xf1, yf0, zf1 ), xs ),
            Lerp( GetGradientDotCommon( HashPrimes( seed, x0, y1, z1 ), xf0, yf1, zf1 ), GetGradientDotCommon( HashPrimes( seed, x1, y1, z1 ), xf1, yf1, zf1 ), xs ), ys ), zs );

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    // Perlin 4D keeps more values live than there are registers, on AVX2/AVX512 evaluating each native half of the
    // 2x wide float32v separately spills less. SSE (and Perlin 3D, Value) measured faster at full width
    template<typename F, typename I>
    FS_FORCEINLINE static F GenPerlin4D( I seed, F x, F y, F z, F w )
    {
        F xs = FS::Floor( x );
        F ys = FS::Floor( y );
        F zs = FS::Floor( z );
        F ws = FS::Floor( w );

        I x0 = FloorToInt( x, xs ) * I( Primes::X );
        I y0 = FloorToInt( y, ys ) * I( Primes::Y );
        I z0 = FloorToInt( z, zs ) * I( Primes::Z );
        I w0 = FloorToInt( w, ws ) * I( Primes::W );
        I x1 = x0 + I( Primes::X );
        I y1 = y0 + I( Primes::Y );
        I z1 = z0 + I( Primes::Z );
        I w1 = w0 + I( Primes::W );

        F xf0 = xs = x - xs;
        F yf0 = ys = y - ys;
        F zf0 = zs = z - zs;
        F wf0 = ws = w - ws;
        F xf1 = xf0 - F( 1 );
        F yf1 = yf0 - F( 1 );
        F zf1 = zf0 - F( 1 );
        F wf1 = wf0 - F( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );
        zs = InterpQuintic( zs );
        ws = InterpQuintic( ws );

        return Lerp( Lerp( Lerp(
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
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z, w );

        float32v value;
        if constexpr( FS::IsNativeV<float32v> || !( SIMD & FastSIMD::FeatureFlag::AVX2 ) )
        {
            value = GenPerlin4D( seed, x, y, z, w );
        }
        else
        {
            value = float32v{ GenPerlin4D( seed.v0, x.v0, y.v0, z.v0, w.v0 ), GenPerlin4D( seed.v1, x.v1, y.v1, z.v1, w.v1 ) };
        }

        constexpr float kBounding = 1.33858621120452880859375;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }
};
