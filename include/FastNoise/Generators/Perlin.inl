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

        // Corner-major path for lane-coherent vectors (uniform grids, AVX2/AVX512): all lanes share y and z, hash the lattice block and decode
        // its gradients once, fold the y/z terms of the dot products into the block and permute them per lane. Gradients have two +-1
        // components so the dot product rounds the same as GetGradientDotCommon
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, FS::Cast<std::int32_t>( y ), FS::Cast<std::int32_t>( z ) ) )
            {
                auto seedB = LatticeBlockBroadcast( seed );
                auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
                auto yp = LatticeBlockPrimed<1>( yi, Primes::Y );
                auto hashZ0 = HashPrimes( seedB, xp, yp, LatticeBlockPrimed<2, 0>( zi, Primes::Z ) );

                using NF = decltype( FS::Cast<float>( hashZ0 ) );
                NF one( 1 ), zero( 0 );

                // y/z offsets per block lane: f0 or f1 = f0 - 1, same ops as the per lane path
                auto blockY = LatticeBlockPrimed<1>( yi, 1 ) - LatticeBlockBroadcast( yi );
                auto blockZ0 = LatticeBlockPrimed<2, 0>( zi, 1 ) - LatticeBlockBroadcast( zi );
                NF fY = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( yf0 ) ) ) - FS::Convert<float>( blockY );
                NF fZ = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( zf0 ) ) );
                NF fZ0 = fZ - FS::Convert<float>( blockZ0 );

                // AVX512: the whole 4 x 2 x 2 block fits one register, z1 corners are at index + 8
                auto hashZ1 = hashZ0;
                NF fZ1 = fZ0;
                int32v xOffsetZ1 = xOffset + int32v( 8 );
                if constexpr( NF::ElementCount < 16 )
                {
                    hashZ1 = HashPrimes( seedB, xp, yp, LatticeBlockPrimed<2, 1>( zi, Primes::Z ) );
                    fZ1 = fZ - FS::Convert<float>( LatticeBlockPrimed<2, 1>( zi, 1 ) - LatticeBlockBroadcast( zi ) );
                    xOffsetZ1 = xOffset;
                }

                // x gradient and y/z part of the dot product: FMulAdd( gX, fX, FMulAdd( fY, gY, fZ * gZ ) )
                NF gXZ0 = GetGradientDotCommon( hashZ0, one, zero, zero );
                NF gXZ1 = GetGradientDotCommon( hashZ1, one, zero, zero );
                NF dotYZZ0 = FS::FMulAdd( fY, GetGradientDotCommon( hashZ0, zero, one, zero ), fZ0 * GetGradientDotCommon( hashZ0, zero, zero, one ) );
                NF dotYZZ1 = FS::FMulAdd( fY, GetGradientDotCommon( hashZ1, zero, one, zero ), fZ1 * GetGradientDotCommon( hashZ1, zero, zero, one ) );

                auto dot = [&]( const NF& gX, const NF& dotYZ, const int32v& idx, const float32v& fX )
                {
                    return FS::FMulAdd( LatticeBlockPermute<float32v>( gX, idx ), fX, LatticeBlockPermute<float32v>( dotYZ, idx ) );
                };

                float32v value = Lerp( Lerp(
                    Lerp( dot( gXZ0, dotYZZ0, xOffset, xf0 ), dot( gXZ0, dotYZZ0, xOffset + int32v( 1 ), xf1 ), xs ),
                    Lerp( dot( gXZ0, dotYZZ0, xOffset + int32v( 4 ), xf0 ), dot( gXZ0, dotYZZ0, xOffset + int32v( 5 ), xf1 ), xs ), ys ),
                    Lerp(
                    Lerp( dot( gXZ1, dotYZZ1, xOffsetZ1, xf0 ), dot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 1 ), xf1 ), xs ),
                    Lerp( dot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 4 ), xf0 ), dot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 5 ), xf1 ), xs ), ys ), zs );

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
