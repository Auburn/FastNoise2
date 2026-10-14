#include "DomainWarpFractal.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Fractal<DomainWarp>, SIMD> : public virtual Fractal<DomainWarp>, public DispatchClass<Generator, SIMD>
{
protected:
    // Progressive samples each octave's warp at the already warped position, Independent always samples at the input position
    // Static taking the final class so member reads use fixed offsets instead of going through the virtual base
    template<bool Progressive, typename Self, size_t... I, typename... P>
    static FS_FORCEINLINE float32v GenWarpFractal( const Self& self, std::index_sequence<I...>, int32v seed, P... pos )
    {
        float32v warpPos[] = { pos... };
        auto* warp = self.GetSourceSIMD( self.mSource );

        float32v amp = self.GetSourceValue( warp->GetWarpAmplitude(), seed, pos... );

        // amp stays 0 through the octaves, skip warping if a generator sourced amplitude is 0 in every lane
        if( warp->GetWarpAmplitude().simdGeneratorPtr && !FS::AnyMask( amp != float32v( 0 ) ) )
        {
            return self.GetSourceValue( warp->GetWarpSource(), seed, pos... );
        }

        float32v weightedStrength = self.GetSourceValue( self.mWeightedStrength, seed, pos... );
        float32v freq = float32v( warp->GetWarpFrequency() );
        int32v seedInc = seed;

        float32v gain = self.GetSourceValue( self.mGain, seed, pos... );
        float32v lacunarity( self.mLacunarity );

        float32v strength = warp->CallWarp( warp, seedInc, amp, ( ( Progressive ? warpPos[I] : pos ) * freq )..., warpPos );

        // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
        bool isWeighted = self.mWeightedStrength.simdGeneratorPtr || self.mWeightedStrength.constant != 0.0f;

        for( int i = 1; i < self.mOctaves; i++ )
        {
            seedInc -= int32v( -1 );
            freq *= lacunarity;
            if( isWeighted )
            {
                amp *= Lerp( float32v( 1 ), float32v( 1 ) - FastLengthSqrt( strength ), weightedStrength );
                amp *= gain;
            }
            else
            {
                amp *= gain;
            }
            strength = warp->CallWarp( warp, seedInc, amp, ( ( Progressive ? warpPos[I] : pos ) * freq )..., warpPos );
        }

        return self.GetSourceValue( warp->GetWarpSource(), seed, warpPos[I]... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpFractalProgressive, SIMD> final : public virtual DomainWarpFractalProgressive, public DispatchClass<Fractal<DomainWarp>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->template GenWarpFractal<true>( *this, std::index_sequence_for<P...>(), seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpFractalIndependent, SIMD> final : public virtual DomainWarpFractalIndependent, public DispatchClass<Fractal<DomainWarp>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->template GenWarpFractal<false>( *this, std::index_sequence_for<P...>(), seed, pos... );
    }
};
