#include "DomainWarp.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarp, SIMD> : public virtual DomainWarp, public DispatchClass<Seeded<ScalableGenerator>, SIMD>
{
protected:
    // Gen of the final warp classes: FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> ), so Warp() is not a virtual call.
    // Members are read through the final class, at fixed offsets instead of through the virtual base table
    template<typename WarpT, typename... P>
    FS_FORCEINLINE float32v WarpGenT( int32v seed, P... pos ) const
    {
        const WarpT& self = static_cast<const WarpT&>( *this );
        float32v warpAmp = float32v( self.mWarpAmplitude.constant );

        if( self.mWarpAmplitude.simdGeneratorPtr )
        {
            warpAmp = self.GetSourceValue( self.mWarpAmplitude, seed, pos... );

            // Warping by 0 doesn't move the position, skip it if the amplitude is 0 in every lane (e.g. masked regions)
            if( !FS::AnyMask( warpAmp != float32v( 0 ) ) )
            {
                return self.GetSourceValue( self.mSource, seed, pos... );
            }
        }

        // Seed offset is applied inside Warp() so it also applies when called from DomainWarpFractal
        self.Warp( seed, warpAmp, ( pos * float32v( self.mFrequency ) )..., pos... );

        return self.GetSourceValue( self.mSource, seed, pos...);
    }

    // GenBlock of the final warp classes, the same values as WarpGenT: the positions are warped per vector,
    // then the source is evaluated for the whole block
    template<typename WarpT, size_t DIMS>
    FS_FORCEINLINE bool WarpGenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        if( !this->SourceHasGenBlock( mSource ) )
        {
            return false;
        }

        WarpGenBlockWarped<WarpT, DIMS>( seed, pos, out, count );
        return true;
    }

    template<typename WarpT, size_t DIMS>
    FASTNOISE_NOINLINE void WarpGenBlockWarped( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        alignas( 64 ) float warpAmpBuffer[DispatchClass<Generator, SIMD>::kGenBlockSize];
        auto warpAmp = this->template GetSourceBlockValues<DIMS>( mWarpAmplitude, seed, pos, warpAmpBuffer, count );
        bool generatorAmp = mWarpAmplitude.simdGeneratorPtr;
        float32v frequency( this->mFrequency );

        this->template GetSourceBlockAt<DIMS, DIMS>( mSource, seed, pos, out, count, [&]( float* dst, intptr_t i, auto... p ) {
            float32v amp = warpAmp[i];

            // Warping by 0 doesn't move the position, as in WarpGenT
            if( !generatorAmp || FS::AnyMask( amp != float32v( 0 ) ) )
            {
                static_cast<const WarpT*>( this )->Warp( seed, amp, ( p * frequency )..., p... );
            }
            this->StoreBlockVector( dst, p... );
        } );
    }

public:
    using GenResultT = typename DispatchClass<Generator, SIMD>::GenResult;

    // Warp() of the final class through a static function, DomainWarpFractal calls it per octave: no virtual call, the strength
    // is returned in registers instead of through a hidden pointer and the positions are updated through one pointer
    static FS_FORCEINLINE float32v CallWarp( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v* out )
    {
        GenResultT result = warp->mWarpFn2D( warp, seed, warpAmp, x, y, out );
        return float32v( result.v0, result.v1 );
    }

    static FS_FORCEINLINE float32v CallWarp( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v* out )
    {
        GenResultT result = warp->mWarpFn3D( warp, seed, warpAmp, x, y, z, out );
        return float32v( result.v0, result.v1 );
    }

    static FS_FORCEINLINE float32v CallWarp( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v* out )
    {
        GenResultT result = warp->mWarpFn4D( warp, seed, warpAmp, x, y, z, w, out );
        return float32v( result.v0, result.v1 );
    }

protected:
    // Called by the constructor of the final class (FASTNOISE_IMPL_GEN_FN)
    template<typename WarpT>
    void InitDispatchFns()
    {
        mWarpFn2D = &WarpFn<WarpT>;
        mWarpFn3D = &WarpFn<WarpT>;
        mWarpFn4D = &WarpFn<WarpT>;
    }

private:
    using WarpFn2D = GenResultT( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v, float32v, float32v* );
    using WarpFn3D = GenResultT( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v, float32v, float32v, float32v* );
    using WarpFn4D = GenResultT( FS_VECTORCALL* )( const DispatchClass*, int32v, float32v, float32v, float32v, float32v, float32v, float32v* );

    template<typename WarpT>
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL WarpFn( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v* out )
    {
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const WarpT*>( warp )->WarpT::Warp( seed, warpAmp, x, y, out[0], out[1] ) );
    }

    template<typename WarpT>
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL WarpFn( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v* out )
    {
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const WarpT*>( warp )->WarpT::Warp( seed, warpAmp, x, y, z, out[0], out[1], out[2] ) );
    }

    template<typename WarpT>
    FASTNOISE_GEN_FN_ATTRIBUTES static GenResultT FS_VECTORCALL WarpFn( const DispatchClass* warp, int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v* out )
    {
        return DispatchClass<Generator, SIMD>::ToGenResult( static_cast<const WarpT*>( warp )->WarpT::Warp( seed, warpAmp, x, y, z, w, out[0], out[1], out[2], out[3] ) );
    }

    WarpFn2D mWarpFn2D = nullptr;
    WarpFn3D mWarpFn3D = nullptr;
    WarpFn4D mWarpFn4D = nullptr;

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
    FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> );

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        return this->template WarpGenBlock<DispatchClass, DIMS>( seed, pos, out, count );
    }

public:
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );
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

    // x from the folded hash (see FoldHash()), its high half is y: the shift is shared
    #define GRADIENT_COORD( _x, _y )\
        int32v hash##_x##_y = HashPrimesHB( seed, x##_x, y##_y );\
        int32v hashHi##_x##_y = FS::BitShiftRightZeroExtend( hash##_x##_y, 16 );\
        float32v contrib##_x##_y = xs##_x * ys##_y;\
        xWarp = FN_FMulAdd( contrib##_x##_y, FS::Convert<float>( ( hash##_x##_y ^ hashHi##_x##_y ) & int32v( 0xffff ) ), xWarp );\
        yWarp = FN_FMulAdd( contrib##_x##_y, FS::Convert<float>( hashHi##_x##_y ), yWarp )

        int32v hash00 = HashPrimesHB( seed, x0, y0 );
        int32v hashHi00 = FS::BitShiftRightZeroExtend( hash00, 16 );
        float32v contrib00 = xs0 * ys0;
        float32v xWarp = contrib00 * FS::Convert<float>( ( hash00 ^ hashHi00 ) & int32v( 0xffff ) );
        float32v yWarp = contrib00 * FS::Convert<float>( hashHi00 );

        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        xWarp = FN_FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FN_FMulSub( yWarp, normalise, float32v( 1 ) );

        xOut = FN_FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FN_FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );

        return FN_FMulAdd( xWarp, xWarp, yWarp * yWarp );
    }
            
    FN_ALIGN_STACK FS_FORCEINLINE float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );
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
        int32v hash##_x##_y##_z = HashPrimesFolded( seed, x##_x, y##_y, z##_z );\
        float32v contrib##_x##_y##_z = xs##_x * ys##_y * zs##_z;\
        xWarp = FN_FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( hash##_x##_y##_z & int32v( 0x3ff ) ), xWarp );\
        yWarp = FN_FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( (hash##_x##_y##_z >> 11) & int32v( 0x3ff ) ), yWarp );\
        zWarp = FN_FMulAdd( contrib##_x##_y##_z, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z, 22 ) ), zWarp )

        int32v hash000 = HashPrimesFolded( seed, x0, y0, z0 );
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

        xWarp = FN_FMulSub( xWarp, normalise, float32v( 1 ) );
        yWarp = FN_FMulSub( yWarp, normalise, float32v( 1 ) );
        zWarp = FN_FMulSub( zWarp, normalise, float32v( 1 ) );

        xOut = FN_FMulAdd( xWarp, warpAmp * float32v( this->mAxisScale[0] ), xOut );
        yOut = FN_FMulAdd( yWarp, warpAmp * float32v( this->mAxisScale[1] ), yOut );
        zOut = FN_FMulAdd( zWarp, warpAmp * float32v( this->mAxisScale[2] ), zOut );

        return FN_FMulAdd( xWarp, xWarp, FN_FMulAdd( yWarp, yWarp, zWarp * zWarp ) );
    }
            
    FN_ALIGN_STACK FS_FORCEINLINE float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        seed += int32v( SeedSteps( mSeedOffset ) );

        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX512_F ) || !( SIMD & ( FastSIMD::FeatureFlag::AVX2 | FastSIMD::FeatureFlag::ARM ) ) )
        {
            return WarpImpl( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
        else // Process each native register half separately, too many live values to fit in registers otherwise (AVX2, NEON)
        {
            auto value0 = WarpImpl( seed.v0, warpAmp.v0, x.v0, y.v0, z.v0, w.v0, xOut.v0, yOut.v0, zOut.v0, wOut.v0 );
            auto value1 = WarpImpl( seed.v1, warpAmp.v1, x.v1, y.v1, z.v1, w.v1, xOut.v1, yOut.v1, zOut.v1, wOut.v1 );
            return float32v( value0, value1 );
        }
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
        auto hashZ0 = HashPrimesFolded( seedB, xp, yp, LatticeBlockPrimed<2, 0>( zi, Primes::Z ) );
        auto hashZ1 = hashZ0;
        int32v xOffsetZ1 = xOffset + int32v( 8 );

        // AVX512: the whole block fits one register, z1 corners are at index + 8
        using N = decltype( hashZ0 );
        if constexpr( N::ElementCount < 16 )
        {
            hashZ1 = HashPrimesFolded( seedB, xp, yp, LatticeBlockPrimed<2, 1>( zi, Primes::Z ) );
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

    // Width generic copy of InterpHermite from Utils.inl, used on native register halves
    template<typename F>
    FS_FORCEINLINE static F InterpHermiteHalf( F t )
    {
        return t * t * FN_FNMulAdd( t, F( 2 ), F( 3 ) );
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

    // x and y from the squared hash, its top bits depend on every hash bit; z and w from the top of the hash itself
    #define GRADIENT_COORD( _x, _y, _z, _w )\
        I hash##_x##_y##_z##_w = HashPrimesHB( seed, x##_x, y##_y, z##_z, w##_w );\
        I hashSq##_x##_y##_z##_w = hash##_x##_y##_z##_w * hash##_x##_y##_z##_w;\
        F contrib##_x##_y##_z##_w = xs##_x * ys##_y * zs##_z * ws##_w;\
        xWarp = FN_FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( FS::BitShiftRightZeroExtend( hashSq##_x##_y##_z##_w, 24 ) ), xWarp );\
        yWarp = FN_FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hashSq##_x##_y##_z##_w >> 16) & I( 0xff ) ), yWarp );\
        zWarp = FN_FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( (hash##_x##_y##_z##_w >> 16) & I( 0xff ) ), zWarp );\
        wWarp = FN_FMulAdd( contrib##_x##_y##_z##_w, FS::Convert<float>( FS::BitShiftRightZeroExtend( hash##_x##_y##_z##_w, 24 ) ), wWarp )

        I hash0000 = HashPrimesHB( seed, x0, y0, z0, w0 );
        I hashSq0000 = hash0000 * hash0000;
        F contrib0000 = xs0 * ys0 * zs0 * ws0;
        F xWarp = contrib0000 * FS::Convert<float>( FS::BitShiftRightZeroExtend( hashSq0000, 24 ) );
        F yWarp = contrib0000 * FS::Convert<float>( (hashSq0000 >> 16) & I( 0xff ) );
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

        xWarp = FN_FMulSub( xWarp, normalise, F( 1 ) );
        yWarp = FN_FMulSub( yWarp, normalise, F( 1 ) );
        zWarp = FN_FMulSub( zWarp, normalise, F( 1 ) );
        wWarp = FN_FMulSub( wWarp, normalise, F( 1 ) );

        xOut = FN_FMulAdd( xWarp, warpAmp * F( this->mAxisScale[0] ), xOut );
        yOut = FN_FMulAdd( yWarp, warpAmp * F( this->mAxisScale[1] ), yOut );
        zOut = FN_FMulAdd( zWarp, warpAmp * F( this->mAxisScale[2] ), zOut );
        wOut = FN_FMulAdd( wWarp, warpAmp * F( this->mAxisScale[3] ), wOut );

        return FN_FMulAdd( xWarp, xWarp, FN_FMulAdd( yWarp, yWarp, FN_FMulAdd( zWarp, zWarp, wWarp * wWarp ) ) );
    }
};

