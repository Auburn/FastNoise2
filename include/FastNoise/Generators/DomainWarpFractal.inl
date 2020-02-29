#include "FastSIMD/InlInclude.h"

#include "DomainWarpFractal.h"

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalProgressive, FS> : public virtual FastNoise::DomainWarpFractalProgressive, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos )
    {
        float32v amp = float32v( this->mSource[0]->GetWarpAmplitude() * mFractalBounding );
        float32v freq = float32v( this->mSource[0]->GetWarpFrequency() );
        int32v seedInc = seed;

        this->mSource[0]->Warp( seedInc, amp, (pos * freq)..., pos... );

        for (int i = 1; i < mOctaves; i++)
        {
            seedInc -= int32v( -1 );
            freq *= float32v( mLacunarity );
            amp *= float32v( mGain );
            this->mSource[0]->Warp( seedInc, amp, (pos * freq)..., pos... );
        }

        return this->mSource[0]->GetSourceSIMD()->Gen( seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalIndependant, FS> : public virtual FastNoise::DomainWarpFractalIndependant, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos )
    {
        return [&] ( std::remove_reference_t<P>... noisePos, P... warpPos )
        {
            float32v amp = float32v( this->mSource[0]->GetWarpAmplitude() * mFractalBounding );
            float32v freq = float32v( this->mSource[0]->GetWarpFrequency() );
            int32v seedInc = seed;
        
            this->mSource[0]->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
    
            for( int i = 1; i < mOctaves; i++ )
            {
                seedInc -= int32v( -1 );
                freq *= float32v( mLacunarity );
                amp *= float32v( mGain );
                this->mSource[0]->Warp( seedInc, amp, (noisePos * freq)..., warpPos... );
            }
    
            return this->mSource[0]->GetSourceSIMD()->Gen( seed, warpPos... );

        } ( pos..., pos... );
    }
};
