#include "Value.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Value, SIMD> final : public virtual Value, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    // Corner-major path for lane-coherent vectors (uniform grids, AVX2): x/y lerp of one z/w layer of a lattice block table, bit identical
    // to the per lane path. Only used in 3D/4D on AVX2: in 2D and on AVX512 it measured slower than hashing every corner per lane.
    // AArch64 (TBL lookups): 3D 1.00 / 4D 0.93 on grids but 1.12 / 1.08 on random positions on an Apple M1, not enabled
    template<typename T>
    FS_FORCEINLINE static float32v LatticeBlockLerpXY( const T& table, int32v xOffset, float32v xs, float32v ys )
    {
        // Lerp( a, b, t ) = FMulAdd( t, b - a, a ): b - a is taken once per block, x neighbours are adjacent lanes
        using TI = decltype( FS::Cast<std::int32_t>( table ) );
        T delta = LatticeBlockPermute<T>( table, FS::LoadIncremented<TI>() + TI( 1 ) ) - table;
        int32v xOffsetY1 = xOffset + int32v( 4 );

        return Lerp(
            FN_FMulAdd( xs, LatticeBlockPermute<float32v>( delta, xOffset ), LatticeBlockPermute<float32v>( table, xOffset ) ),
            FN_FMulAdd( xs, LatticeBlockPermute<float32v>( delta, xOffsetY1 ), LatticeBlockPermute<float32v>( table, xOffsetY1 ) ), ys );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );
        this->ScalePositions( this, x, y );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v x0 = FloorToInt( x, xs ) * int32v( Primes::X );
        int32v y0 = FloorToInt( y, ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );

        return this->ScaleOutput( this, Lerp(
            Lerp( GetValueCoord( seed, x0, y0 ), GetValueCoord( seed, x1, y0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1 ), GetValueCoord( seed, x1, y1 ), xs ), ys ),
            -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );
        this->ScalePositions( this, x, y, z );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );

        int32v xi = FloorToInt( x, xs );
        int32v yi = FloorToInt( y, ys );
        int32v zi = FloorToInt( z, zs );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );

        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, yi, zi ) )
            {
                auto seedB = LatticeBlockSplat( seed );
                auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
                auto yp = LatticeBlockPrimed<1>( yi, Primes::Y );

                return this->ScaleOutput( this, Lerp(
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, LatticeBlockPrimed<2, 0>( zi, Primes::Z ) ), xOffset, xs, ys ),
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, LatticeBlockPrimed<2, 1>( zi, Primes::Z ) ), xOffset, xs, ys ), zs ),
                    -kValueBounds, kValueBounds );
            }
        }

        int32v x0 = xi * int32v( Primes::X );
        int32v y0 = yi * int32v( Primes::Y );
        int32v z0 = zi * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        return this->ScaleOutput( this, Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0 ), GetValueCoord( seed, x1, y0, z0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0 ), GetValueCoord( seed, x1, y1, z0 ), xs ), ys ),
            Lerp(                                                                                
            Lerp( GetValueCoord( seed, x0, y0, z1 ), GetValueCoord( seed, x1, y0, z1 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1 ), GetValueCoord( seed, x1, y1, z1 ), xs ), ys ), zs ),
            -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );
        this->ScalePositions( this, x, y, z, w );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );
        float32v ws = FS::Floor( w );

        int32v xi = FloorToInt( x, xs );
        int32v yi = FloorToInt( y, ys );
        int32v zi = FloorToInt( z, zs );
        int32v wi = FloorToInt( w, ws );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );
        ws = InterpHermite( w - ws );

        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            int32v xOffset;
            if( IsLatticeBlock( xOffset, seed, xi, yi, zi, wi ) )
            {
                auto seedB = LatticeBlockSplat( seed );
                auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
                auto yp = LatticeBlockPrimed<1>( yi, Primes::Y );
                auto zp0 = LatticeBlockPrimed<2, 0>( zi, Primes::Z );
                auto zp1 = LatticeBlockPrimed<2, 1>( zi, Primes::Z );
                auto wp0 = LatticeBlockPrimed<3, 0>( wi, Primes::W );
                auto wp1 = LatticeBlockPrimed<3, 2>( wi, Primes::W );

                return this->ScaleOutput( this, Lerp( Lerp(
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, zp0, wp0 ), xOffset, xs, ys ),
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, zp1, wp0 ), xOffset, xs, ys ), zs ),
                    Lerp(
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, zp0, wp1 ), xOffset, xs, ys ),
                    LatticeBlockLerpXY( GetValueCoord( seedB, xp, yp, zp1, wp1 ), xOffset, xs, ys ), zs ), ws ),
                    -kValueBounds, kValueBounds );
            }
        }

        int32v x0 = xi * int32v( Primes::X );
        int32v y0 = yi * int32v( Primes::Y );
        int32v z0 = zi * int32v( Primes::Z );
        int32v w0 = wi * int32v( Primes::W );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );
        int32v w1 = w0 + int32v( Primes::W );

        return this->ScaleOutput( this, Lerp( Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0, w0 ), GetValueCoord( seed, x1, y0, z0, w0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0, w0 ), GetValueCoord( seed, x1, y1, z0, w0 ), xs ), ys ),
            Lerp( 
            Lerp( GetValueCoord( seed, x0, y0, z1, w0 ), GetValueCoord( seed, x1, y0, z1, w0 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1, w0 ), GetValueCoord( seed, x1, y1, z1, w0 ), xs ), ys ), zs ),
            Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0, w1 ), GetValueCoord( seed, x1, y0, z0, w1 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0, w1 ), GetValueCoord( seed, x1, y1, z0, w1 ), xs ), ys ),
            Lerp( 
            Lerp( GetValueCoord( seed, x0, y0, z1, w1 ), GetValueCoord( seed, x1, y0, z1, w1 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1, w1 ), GetValueCoord( seed, x1, y1, z1, w1 ), xs ), ys ), zs ), ws ),
            -kValueBounds, kValueBounds );
    }
};
