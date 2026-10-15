#include "Perlin.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Perlin, SIMD> final : public virtual Perlin, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    // AVX2/AVX512 lattice blocks: gX * fX + the rest of the dot product, both permuted out of the block (static function, MSVC
    // doesn't always inline lambdas)
    template<typename T>
    FS_FORCEINLINE static float32v LatticeBlockDot( const T& gX, const T& dotRest, const int32v& index, const float32v& fX )
    {
        return FS::FMulAdd( LatticeBlockPermute<float32v>( gX, index ), fX, LatticeBlockPermute<float32v>( dotRest, index ) );
    }

    // SSE4.1 lattice blocks: gX * fX + the rest of the dot product, both looked up from one x row of 4 corners
    template<typename T>
    FS_FORCEINLINE static float32v LatticeRowDot( const T& gX, const T& dotRest, const int32v& byteIndex, const float32v& fX )
    {
        return FS::FMulAdd( LatticeRowPermute<float32v>( gX, byteIndex ), fX, LatticeRowPermute<float32v>( dotRest, byteIndex ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v xi = FloorToInt( x, xs );
        int32v yi = FloorToInt( y, ys );

        float32v xf0 = xs = x - xs;
        float32v yf0 = ys = y - ys;
        float32v xf1 = xf0 - float32v( 1 );
        float32v yf1 = yf0 - float32v( 1 );

        xs = InterpQuintic( xs );
        ys = InterpQuintic( ys );

        constexpr float kBounding = 1.726796627044677734375f;

        // Corner-major path for lane-coherent vectors, see 3D. AVX512 measured slower on fractals: its 32 lanes span more than
        // a cell from the second octave on, so the gate mostly falls through
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, FS::Cast<std::int32_t>( y ) ) )
            {
                auto hash = HashPrimes( LatticeBlockBroadcast( seed ), LatticeBlockPrimed<0>( xi, Primes::X ), LatticeBlockPrimed<1>( yi, Primes::Y ) );

                using NF = decltype( FS::Cast<float>( hash ) );
                NF one( 1 ), zero( 0 );

                // y offset per block lane: yf0 or yf1 = yf0 - 1, same ops as the per lane path
                auto blockY = LatticeBlockPrimed<1>( yi, 1 ) - LatticeBlockBroadcast( yi );
                NF fY = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( yf0 ) ) ) - FS::Convert<float>( blockY );

                // FMulAdd( gX, fX, fY * gY )
                NF gX = GetGradientDotPerlin( hash, one, zero );
                NF dotY = fY * GetGradientDotPerlin( hash, zero, one );

                float32v value = Lerp(
                    Lerp( LatticeBlockDot( gX, dotY, xOffset, xf0 ), LatticeBlockDot( gX, dotY, xOffset + int32v( 1 ), xf1 ), xs ),
                    Lerp( LatticeBlockDot( gX, dotY, xOffset + int32v( 4 ), xf0 ), LatticeBlockDot( gX, dotY, xOffset + int32v( 5 ), xf1 ), xs ), ys );

                return this->ScaleOutput( this, value, -kBounding, kBounding );
            }
        }

        int32v x0 = xi * int32v( Primes::X );
        int32v y0 = yi * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        float32v value = Lerp(
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y0 ), xf0, yf0 ), GetGradientDotPerlin( HashPrimes( seed, x1, y0 ), xf1, yf0 ), xs ),
            Lerp( GetGradientDotPerlin( HashPrimes( seed, x0, y1 ), xf0, yf1 ), GetGradientDotPerlin( HashPrimes( seed, x1, y1 ), xf1, yf1 ), xs ), ys );

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

                float32v value = Lerp( Lerp(
                    Lerp( LatticeBlockDot( gXZ0, dotYZZ0, xOffset, xf0 ), LatticeBlockDot( gXZ0, dotYZZ0, xOffset + int32v( 1 ), xf1 ), xs ),
                    Lerp( LatticeBlockDot( gXZ0, dotYZZ0, xOffset + int32v( 4 ), xf0 ), LatticeBlockDot( gXZ0, dotYZZ0, xOffset + int32v( 5 ), xf1 ), xs ), ys ),
                    Lerp(
                    Lerp( LatticeBlockDot( gXZ1, dotYZZ1, xOffsetZ1, xf0 ), LatticeBlockDot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 1 ), xf1 ), xs ),
                    Lerp( LatticeBlockDot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 4 ), xf0 ), LatticeBlockDot( gXZ1, dotYZZ1, xOffsetZ1 + int32v( 5 ), xf1 ), xs ), ys ), zs );

                return this->ScaleOutput( this, value, -kBounding, kBounding );
            }
        }
        // SSE4.1: same with one register per x row of 4 block corners, looked up with pshufb. The SSE gradient dot product is a
        // single rounded sum of two signed components, so it equals gX * fX + ( y/z part )
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::SSE41 ) && !( SIMD & FastSIMD::FeatureFlag::AVX ) )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, FS::Cast<std::int32_t>( y ), FS::Cast<std::int32_t>( z ) ) )
            {
                auto seedB = LatticeBlockBroadcast( seed );
                auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
                auto yp0 = LatticeBlockPrimed<1, 0>( yi, Primes::Y );
                auto zp0 = LatticeBlockPrimed<2, 0>( zi, Primes::Z );
                auto yp1 = LatticeBlockPrimed<1, 1>( yi, Primes::Y );
                auto zp1 = LatticeBlockPrimed<2, 2>( zi, Primes::Z );

                using NF = decltype( FS::Cast<float>( xp ) );
                NF one( 1 ), zero( 0 );

                // y/z offsets per row: f0 or f1 = f0 - 1, same ops as the per lane path
                NF fY0 = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( yf0 ) ) );
                NF fZ0 = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( zf0 ) ) );
                NF fY1 = fY0 - NF( 1 );
                NF fZ1 = fZ0 - NF( 1 );

                auto hash00 = HashPrimes( seedB, xp, yp0, zp0 );
                auto hash10 = HashPrimes( seedB, xp, yp1, zp0 );
                auto hash01 = HashPrimes( seedB, xp, yp0, zp1 );
                auto hash11 = HashPrimes( seedB, xp, yp1, zp1 );

                NF gX00 = GetGradientDotCommon( hash00, one, zero, zero );
                NF gX10 = GetGradientDotCommon( hash10, one, zero, zero );
                NF gX01 = GetGradientDotCommon( hash01, one, zero, zero );
                NF gX11 = GetGradientDotCommon( hash11, one, zero, zero );
                NF dotYZ00 = GetGradientDotCommon( hash00, zero, fY0, fZ0 );
                NF dotYZ10 = GetGradientDotCommon( hash10, zero, fY1, fZ0 );
                NF dotYZ01 = GetGradientDotCommon( hash01, zero, fY0, fZ1 );
                NF dotYZ11 = GetGradientDotCommon( hash11, zero, fY1, fZ1 );

                int32v idx0 = LatticeRowByteIndex( xOffset );
                int32v idx1 = idx0 + int32v( 0x04040404 );

                float32v value = Lerp( Lerp(
                    Lerp( LatticeRowDot( gX00, dotYZ00, idx0, xf0 ), LatticeRowDot( gX00, dotYZ00, idx1, xf1 ), xs ),
                    Lerp( LatticeRowDot( gX10, dotYZ10, idx0, xf0 ), LatticeRowDot( gX10, dotYZ10, idx1, xf1 ), xs ), ys ),
                    Lerp(
                    Lerp( LatticeRowDot( gX01, dotYZ01, idx0, xf0 ), LatticeRowDot( gX01, dotYZ01, idx1, xf1 ), xs ),
                    Lerp( LatticeRowDot( gX11, dotYZ11, idx0, xf0 ), LatticeRowDot( gX11, dotYZ11, idx1, xf1 ), xs ), ys ), zs );

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

        if constexpr( FS::IsNativeV<F> && ( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 ) )
        {
            I xi = FloorToInt( x, xs );
            I xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, FS::Cast<std::int32_t>( y ), FS::Cast<std::int32_t>( z ), FS::Cast<std::int32_t>( w ) ) )
            {
                return GenPerlin4DBlock( seed, xOffset, xi, FloorToInt( y, ys ), FloorToInt( z, zs ), FloorToInt( w, ws ), x - xs, y - ys, z - zs, w - ws );
            }
        }
        else if constexpr( ( F::FeatureFlags & FastSIMD::FeatureFlag::SSE41 ) && !( F::FeatureFlags & FastSIMD::FeatureFlag::AVX ) )
        {
            I xi = FloorToInt( x, xs );
            I xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, FS::Cast<std::int32_t>( y ), FS::Cast<std::int32_t>( z ), FS::Cast<std::int32_t>( w ) ) )
            {
                return GenPerlin4DRows( seed, xOffset, xi, FloorToInt( y, ys ), FloorToInt( z, zs ), FloorToInt( w, ws ), x - xs, y - ys, z - zs, w - ws );
            }
        }

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

    // Corner-major Perlin 4D for a native register whose lanes share seed and y/z/w and span at most 3 x cells (uniform grids):
    // hash the 4 x 2 x 2 x 2 lattice block once and fold the y/z/w part of each corner's dot product into tables, so that each
    // lane does the same roundings as GetGradientDotPerlin
    template<typename F, typename I>
    FS_FORCEINLINE static F GenPerlin4DBlock( I seed, I xOffset, I xi, I yi, I zi, I wi, F xf0, F yf0, F zf0, F wf0 )
    {
        F xf1 = xf0 - F( 1 );
        F zf1 = zf0 - F( 1 );
        F wf1 = wf0 - F( 1 );

        F xs = InterpQuintic( xf0 );
        F ys = InterpQuintic( yf0 );
        F zs = InterpQuintic( zf0 );
        F ws = InterpQuintic( wf0 );

        I xp = LatticeBlockPrimed<0>( xi, Primes::X );
        I yp = LatticeBlockPrimed<1>( yi, Primes::Y );
        I w0 = wi * I( Primes::W );
        I w1 = w0 + I( Primes::W );

        // y offset per block lane: yf0 or yf1 = yf0 - 1, same op as the per lane path
        F fY = yf0 - FS::Convert<float>( LatticeBlockPrimed<1>( yi, 1 ) - yi );

        if constexpr( F::ElementCount == 16 )
        {
            // One register per w layer, lane dx + 4 dy + 8 dz
            I zp = LatticeBlockPrimed<2>( zi, Primes::Z );
            F fZ = zf0 - FS::Convert<float>( LatticeBlockPrimed<2>( zi, 1 ) - zi );

            return Lerp(
                Perlin4DBlockLayerAVX512( HashPrimes( seed, xp, yp, zp, w0 ), xOffset, fY, fZ, wf0, xf0, xf1, xs, ys, zs ),
                Perlin4DBlockLayerAVX512( HashPrimes( seed, xp, yp, zp, w1 ), xOffset, fY, fZ, wf1, xf0, xf1, xs, ys, zs ), ws );
        }
        else
        {
            // One register per z/w layer, lane dx + 4 dy
            I z0 = zi * I( Primes::Z );
            I z1 = z0 + I( Primes::Z );

            return Lerp(
                Lerp( Perlin4DBlockLayerAVX2( HashPrimes( seed, xp, yp, z0, w0 ), xOffset, fY, zf0, wf0, xf0, xf1, xs, ys ),
                      Perlin4DBlockLayerAVX2( HashPrimes( seed, xp, yp, z1, w0 ), xOffset, fY, zf1, wf0, xf0, xf1, xs, ys ), zs ),
                Lerp( Perlin4DBlockLayerAVX2( HashPrimes( seed, xp, yp, z0, w1 ), xOffset, fY, zf0, wf1, xf0, xf1, xs, ys ),
                      Perlin4DBlockLayerAVX2( HashPrimes( seed, xp, yp, z1, w1 ), xOffset, fY, zf1, wf1, xf0, xf1, xs, ys ), zs ), ws );
        }
    }

    // AVX512 block layer: dot = FMulAdd( gX, fX, FMulAdd( fY, gY, FMulAdd( fZ, gZ, fW * gW ) ) ), the y/z/w part per block lane
    template<typename F, typename I>
    FS_FORCEINLINE static F Perlin4DBlockLayerAVX512( I hash, I xOffset, F fY, F fZ, F fW, F xf0, F xf1, F xs, F ys, F zs )
    {
        F gX, gY, gZ, gW;
        GetGradientPerlin4DAVX512( hash, gX, gY, gZ, gW );
        F dotYZW = FS::FMulAdd( fY, gY, FS::FMulAdd( fZ, gZ, fW * gW ) );

        return Lerp( Lerp(
            Lerp( Perlin4DBlockDotAVX512( gX, dotYZW, xOffset, xf0 ), Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 1 ), xf1 ), xs ),
            Lerp( Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 4 ), xf0 ), Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 5 ), xf1 ), xs ), ys ),
            Lerp(
            Lerp( Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 8 ), xf0 ), Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 9 ), xf1 ), xs ),
            Lerp( Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 12 ), xf0 ), Perlin4DBlockDotAVX512( gX, dotYZW, xOffset + I( 13 ), xf1 ), xs ), ys ), zs );
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F Perlin4DBlockDotAVX512( const F& gX, const F& dotYZW, const I& index, const F& fX )
    {
        return FS::FMulAdd( LatticeBlockPermute<F>( gX, index ), fX, LatticeBlockPermute<F>( dotYZW, index ) );
    }

    // AVX2 block layer: the lane dot is FMulAdd( c, cSign, FMulAdd( b, bSign, a * aSign ) ) with a = x if the gradient has an x
    // component (p > 0), else y. Per lane FMulAdd( fX, sA, tB ) + tC rounds the same: sA = aSign or 0, tB = b * bSign or the whole
    // inner term when x is absent, tC = c * cSign (products with +/-1 are exact). Only the sign of zero results can differ,
    // ScaleOutput's + kBounding removes it
    template<typename F, typename I>
    FS_FORCEINLINE static F Perlin4DBlockLayerAVX2( I hash, I xOffset, F fY, F fZ, F fW, F xf0, F xf1, F xs, F ys )
    {
        I p = hash & I( 3 << 3 );
        auto hasX = p > I( 0 );

        F b = FS::SelectHighBit( hash << 27, fY, fZ );
        F c = FS::Select( p > I( 2 << 3 ), fZ, fW );
        F aSign, bSign, cSign;
        GetGradientSignsPerlin4DAVX2( hash, aSign, bSign, cSign );

        F sA = FS::Masked( hasX, aSign );
        F tB = FS::FMulAdd( b, bSign, FS::InvMasked( hasX, fY * aSign ) );
        F tC = c * cSign;

        return Lerp(
            Lerp( Perlin4DBlockDotAVX2( sA, tB, tC, xOffset, xf0 ), Perlin4DBlockDotAVX2( sA, tB, tC, xOffset + I( 1 ), xf1 ), xs ),
            Lerp( Perlin4DBlockDotAVX2( sA, tB, tC, xOffset + I( 4 ), xf0 ), Perlin4DBlockDotAVX2( sA, tB, tC, xOffset + I( 5 ), xf1 ), xs ), ys );
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F Perlin4DBlockDotAVX2( const F& sA, const F& tB, const F& tC, const I& index, const F& fX )
    {
        return FS::FMulAdd( fX, LatticeBlockPermute<F>( sA, index ), LatticeBlockPermute<F>( tB, index ) ) + LatticeBlockPermute<F>( tC, index );
    }

    // SSE4.1 Perlin 4D lattice block: one register per x row of 4 corners, looked up with pshufb. Each lane adds up
    // FMulAdd( fX, sA, tB ) + tC as on AVX2 (see Perlin4DBlockLayerAVX2), i.e. ( a' + b' ) + c'. Not bit exact: fast-math
    // reassociates the per lane path's ( a' + b' ) + c' differently per corner, so a few results differ in the last bit
    template<typename F, typename I>
    FS_FORCEINLINE static F GenPerlin4DRows( I seed, I xOffset, I xi, I yi, I zi, I wi, F xf0, F yf0, F zf0, F wf0 )
    {
        F xf1 = xf0 - F( 1 );

        F xs = InterpQuintic( xf0 );
        F ys = InterpQuintic( yf0 );
        F zs = InterpQuintic( zf0 );
        F ws = InterpQuintic( wf0 );

        auto seedB = LatticeBlockBroadcast( seed );
        auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
        using NI = decltype( xp );
        using NF = decltype( FS::Cast<float>( xp ) );

        NI yp0 = LatticeBlockPrimed<1, 0>( yi, Primes::Y );
        NI zp0 = LatticeBlockPrimed<2, 0>( zi, Primes::Z );
        NI wp0 = LatticeBlockPrimed<3, 0>( wi, Primes::W );
        NI yp1 = yp0 + NI( Primes::Y );
        NI zp1 = zp0 + NI( Primes::Z );
        NI wp1 = wp0 + NI( Primes::W );

        // y/z/w offsets of the rows: f0 or f1 = f0 - 1, same ops as the per lane path
        NF fY0 = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( yf0 ) ) );
        NF fZ0 = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( zf0 ) ) );
        NF fW0 = FS::Cast<float>( LatticeBlockBroadcast( FS::Cast<std::int32_t>( wf0 ) ) );
        NF fY1 = fY0 - NF( 1 );
        NF fZ1 = fZ0 - NF( 1 );
        NF fW1 = fW0 - NF( 1 );

        I idx0 = LatticeRowByteIndex( xOffset );
        I idx1 = idx0 + I( 0x04040404 );

        return Lerp( Lerp( Lerp(
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp0, zp0, wp0 ), fY0, fZ0, fW0, idx0, idx1, xf0, xf1, xs ),
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp1, zp0, wp0 ), fY1, fZ0, fW0, idx0, idx1, xf0, xf1, xs ), ys ),
            Lerp(
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp0, zp1, wp0 ), fY0, fZ1, fW0, idx0, idx1, xf0, xf1, xs ),
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp1, zp1, wp0 ), fY1, fZ1, fW0, idx0, idx1, xf0, xf1, xs ), ys ), zs ),
            Lerp( Lerp(
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp0, zp0, wp1 ), fY0, fZ0, fW1, idx0, idx1, xf0, xf1, xs ),
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp1, zp0, wp1 ), fY1, fZ0, fW1, idx0, idx1, xf0, xf1, xs ), ys ),
            Lerp(
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp0, zp1, wp1 ), fY0, fZ1, fW1, idx0, idx1, xf0, xf1, xs ),
            Perlin4DRowLerpX( HashPrimes( seedB, xp, yp1, zp1, wp1 ), fY1, fZ1, fW1, idx0, idx1, xf0, xf1, xs ), ys ), zs ), ws );
    }

    // One row of the SSE4.1 block: tables sA (x sign or zero), tB (b' or a' + b' when x is absent), tC (c'), signs applied by xor
    template<typename NI, typename NF, typename F, typename I>
    FS_FORCEINLINE static F Perlin4DRowLerpX( NI hash, NF fY, NF fZ, NF fW, const I& idx0, const I& idx1, const F& xf0, const F& xf1, const F& xs )
    {
        NI p = hash & NI( 3 << 3 );
        auto hasX = p > NI( 0 );

        NF b = FS::SelectHighBit( hash << 27, fY, fZ );
        NF c = FS::Select( p > NI( 2 << 3 ), fZ, fW );
        NF aSign = FS::Cast<float>( hash << 31 );
        NF bSign = FS::Cast<float>( ( hash >> 1 ) << 31 );
        NF cSign = FS::Cast<float>( ( hash >> 2 ) << 31 );

        NF bSigned = b ^ bSign;
        NF sA = FS::Masked( hasX, NF( 1 ) ^ aSign );
        NF tB = FS::Select( hasX, bSigned, ( fY ^ aSign ) + bSigned );
        NF tC = c ^ cSign;

        return Lerp(
            FS::FMulAdd( xf0, LatticeRowPermute<F>( sA, idx0 ), LatticeRowPermute<F>( tB, idx0 ) ) + LatticeRowPermute<F>( tC, idx0 ),
            FS::FMulAdd( xf1, LatticeRowPermute<F>( sA, idx1 ), LatticeRowPermute<F>( tB, idx1 ) ) + LatticeRowPermute<F>( tC, idx1 ), xs );
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
