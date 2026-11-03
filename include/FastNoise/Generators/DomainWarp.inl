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

        // Corner-major path (AVX2 only, on AVX512 the per lane path measured about as fast on grids and the gate costs on other input)
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            int32v xi = FS::Convert<int32_t>( xs );
            int32v yi = FS::Convert<int32_t>( ys );
            int32v zi = FS::Convert<int32_t>( zs );
            int32v xOffset;

            if( IsLatticeBlock( xOffset, seed, xi, yi, zi ) )
            {
                return WarpBlock( seed, warpAmp, xOffset, xi, yi, zi, x - xs, y - ys, z - zs, xOut, yOut, zOut );
            }
        }

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
        float32v contrib##_x##_y##_z##_w = xs##_x * ys##_y * zs##_z * ws##_w;\
        xWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( hash##_x##_y##_z##_w & int32v( 0xff ) ), xWarp );\
        yWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 8) & int32v( 0xff ) ), yWarp );\
        zWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 16) & int32v( 0xff ) ), zWarp );\
        wWarp = FS::FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z##_w, 24 ) ), wWarp )

        int32v hash0000 = HashPrimesHB( seed, x0, y0, z0, w0 );
        float32v contrib0000 = xs0 * ys0 * zs0 * ws0;
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

        xWarp = FS::FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FS::FMulSub( yWarp, normalise, float32v( 1 ) );
        zWarp = FS::FMulSub( zWarp, normalise, float32v( 1 ) );
        wWarp = FS::FMulSub( wWarp, normalise, float32v( 1 ) );

        xOut = FS::FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp * float32v( this->mAxisScale[2] ), zOut );
        wOut = FS::FMulAdd( wWarp, warpAmp * float32v( this->mAxisScale[3] ), wOut );

        return FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, FS::FMulAdd( zWarp, zWarp, wWarp * wWarp ) ) );
    }

private:
    template<typename T>
    FS_FORCEINLINE static void WarpBlockCorner( const T& tableX, const T& tableY, const T& tableZ, float32v contrib, int32v index, float32v& xWarp, float32v& yWarp, float32v& zWarp )
    {
        xWarp = FS::FMulAdd( contrib, LatticeBlockPermute<float32v>( tableX, index ), xWarp );
        yWarp = FS::FMulAdd( contrib, LatticeBlockPermute<float32v>( tableY, index ), yWarp );
        zWarp = FS::FMulAdd( contrib, LatticeBlockPermute<float32v>( tableZ, index ), zWarp );
    }

    // 3D corner-major path for lane-coherent vectors (uniform grids): lanes share seed and floored y/z and span at most 3 x cells.
    // Hash and decode the 4 x 2 x 2 lattice block once (one register on AVX512, z0/z1 registers on AVX2) and permute the decoded
    // corner values per lane, same values and FMA order as the per lane path in Warp
    FS_FORCEINLINE float32v WarpBlock( int32v seed, float32v warpAmp, int32v xOffset, int32v xi, int32v yi, int32v zi, float32v xf, float32v yf, float32v zf, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        float32v xs1 = InterpHermite( xf );
        float32v ys1 = InterpHermite( yf );
        float32v zs1 = InterpHermite( zf );
        float32v xs0 = float32v( 1 ) - xs1;
        float32v ys0 = float32v( 1 ) - ys1;
        float32v zs0 = float32v( 1 ) - zs1;

        float32v normalise( 1.0f / (0x3ff / 2.0f) );

        auto seedB = LatticeBlockBroadcast( seed );
        auto xp = LatticeBlockPrimed<0>( xi, Primes::X );
        auto yp = LatticeBlockPrimed<1>( yi, Primes::Y );
        auto hashZ0 = HashPrimesHB( seedB, xp, yp, LatticeBlockPrimed<2, 0>( zi, Primes::Z ) );
        auto hashZ1 = hashZ0;
        int32v xOffsetZ1 = xOffset + int32v( 8 );

        // AVX512: the whole block fits one register, z1 corners are at index + 8
        using N = decltype( hashZ0 );
        if constexpr( N::ElementCount < 16 )
        {
            hashZ1 = HashPrimesHB( seedB, xp, yp, LatticeBlockPrimed<2, 1>( zi, Primes::Z ) );
            xOffsetZ1 = xOffset;
        }

        auto xZ0 = FS::Convert<float>( hashZ0 & N( 0x3ff ) );
        auto yZ0 = FS::Convert<float>( ( hashZ0 >> 11 ) & N( 0x3ff ) );
        auto zZ0 = FS::Convert<float>( FS::BitShiftRightZeroExtend( hashZ0, 22 ) );

        float32v contrib000 = xs0 * ys0 * zs0;
        float32v xWarp = contrib000 * LatticeBlockPermute<float32v>( xZ0, xOffset );
        float32v yWarp = contrib000 * LatticeBlockPermute<float32v>( yZ0, xOffset );
        float32v zWarp = contrib000 * LatticeBlockPermute<float32v>( zZ0, xOffset );

        WarpBlockCorner( xZ0, yZ0, zZ0, xs1 * ys0 * zs0, xOffset + int32v( 1 ), xWarp, yWarp, zWarp );
        WarpBlockCorner( xZ0, yZ0, zZ0, xs0 * ys1 * zs0, xOffset + int32v( 4 ), xWarp, yWarp, zWarp );
        WarpBlockCorner( xZ0, yZ0, zZ0, xs1 * ys1 * zs0, xOffset + int32v( 5 ), xWarp, yWarp, zWarp );

        auto xZ1 = FS::Convert<float>( hashZ1 & N( 0x3ff ) );
        auto yZ1 = FS::Convert<float>( ( hashZ1 >> 11 ) & N( 0x3ff ) );
        auto zZ1 = FS::Convert<float>( FS::BitShiftRightZeroExtend( hashZ1, 22 ) );

        WarpBlockCorner( xZ1, yZ1, zZ1, xs0 * ys0 * zs1, xOffsetZ1, xWarp, yWarp, zWarp );
        WarpBlockCorner( xZ1, yZ1, zZ1, xs1 * ys0 * zs1, xOffsetZ1 + int32v( 1 ), xWarp, yWarp, zWarp );
        WarpBlockCorner( xZ1, yZ1, zZ1, xs0 * ys1 * zs1, xOffsetZ1 + int32v( 4 ), xWarp, yWarp, zWarp );
        WarpBlockCorner( xZ1, yZ1, zZ1, xs1 * ys1 * zs1, xOffsetZ1 + int32v( 5 ), xWarp, yWarp, zWarp );

        xWarp = FS::FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FS::FMulSub( yWarp, normalise, float32v( 1 ) );
        zWarp = FS::FMulSub( zWarp, normalise, float32v( 1 ) );

        xOut = FS::FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FS::FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );
        zOut = FS::FMulAdd( zWarp, warpAmp * float32v( this->mAxisScale[2] ), zOut );

        return FS::FMulAdd( xWarp, xWarp, FS::FMulAdd( yWarp, yWarp, zWarp * zWarp ) );
    }
};

