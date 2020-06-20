#include "FastSIMD/InlInclude.h"

#include "DomainWarpFractal.h"

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalProgressive, FS> : public virtual FastNoise::DomainWarpFractalProgressive, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v amp = float32v( mFractalBounding ) * this->GetSourceValue( this->GetSourceSIMD( mSource )->GetWarpAmplitude(), seed, pos... );
        float32v freq = float32v( this->GetSourceSIMD( mSource )->GetWarpFrequency() );
        int32v seedInc = seed;

        float32v gain = this->GetSourceValue( mGain, seed, pos... );
        float32v lacunarity( mLacunarity );

        this->GetSourceSIMD( mSource )->Warp( seedInc, amp, (pos * freq)..., pos... );

        for (int i = 1; i < mOctaves; i++)
        {
            seedInc -= int32v( -1 );
            freq *= lacunarity;
            amp *= gain;
            this->GetSourceSIMD( mSource )->Warp( seedInc, amp, (pos * freq)..., pos... );
        }

        return this->GetSourceValue( mSource, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalIndependant, FS> : public virtual FastNoise::DomainWarpFractalIndependant, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return [this, seed] ( std::remove_reference_t<P>... noisePos, std::remove_reference_t<P>... warpPos )
        {
            float32v amp = float32v( mFractalBounding ) * this->GetSourceValue( this->GetSourceSIMD( mSource )->GetWarpAmplitude(), seed, noisePos... );
            float32v freq = float32v( this->GetSourceSIMD( mSource )->GetWarpFrequency() );
            int32v seedInc = seed;

            float32v gain = this->GetSourceValue( mGain, seed, noisePos... );
            float32v lacunarity( mLacunarity );
        
            this->GetSourceSIMD( mSource )->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
    
            for( int i = 1; i < mOctaves; i++ )
            {
                seedInc -= int32v( -1 );
                freq *= lacunarity;
                amp *= gain;
                this->GetSourceSIMD( mSource )->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
            }
    
            return this->GetSourceValue( mSource, seed, warpPos... );

        } ( pos..., pos... );
    }
};
