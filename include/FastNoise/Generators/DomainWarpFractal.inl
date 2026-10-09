#include "DomainWarpFractal.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpFractalProgressive, SIMD> final : public virtual DomainWarpFractalProgressive, public DispatchClass<Fractal<DomainWarp>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        auto* warp = this->GetSourceSIMD( mSource );

        float32v amp = this->GetSourceValue( warp->GetWarpAmplitude(), seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v freq = float32v( warp->GetWarpFrequency() );
        int32v seedInc = seed;

        float32v gain = this->GetSourceValue( mGain, seed, pos... );
        float32v lacunarity( mLacunarity );

        float32v strength = warp->Warp( seedInc, amp, (pos * freq)..., pos... );

        // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
        bool isWeighted = mWeightedStrength.simdGeneratorPtr || mWeightedStrength.constant != 0.0f;

        for (int i = 1; i < mOctaves; i++)
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
            strength = warp->Warp( seedInc, amp, (pos * freq)..., pos... );
        }

        return this->GetSourceValue( warp->GetWarpSource(), seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpFractalIndependent, SIMD> final : public virtual DomainWarpFractalIndependent, public DispatchClass<Fractal<DomainWarp>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return [this, seed] ( std::remove_reference_t<P>... noisePos, std::remove_reference_t<P>... warpPos )
        {
            auto* warp = this->GetSourceSIMD( mSource );

            float32v amp = this->GetSourceValue( warp->GetWarpAmplitude(), seed, noisePos... );
            float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, noisePos... );
            float32v freq = float32v( warp->GetWarpFrequency() );
            int32v seedInc = seed;

            float32v gain = this->GetSourceValue( mGain, seed, noisePos... );
            float32v lacunarity( mLacunarity );
        
            float32v strength = warp->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
    
            // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
            bool isWeighted = mWeightedStrength.simdGeneratorPtr || mWeightedStrength.constant != 0.0f;

            for( int i = 1; i < mOctaves; i++ )
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
                strength = warp->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
            }
    
            return this->GetSourceValue( warp->GetWarpSource(), seed, warpPos... );

        } ( pos..., pos... );
    }
};
