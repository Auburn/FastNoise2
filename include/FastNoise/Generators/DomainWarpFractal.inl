#include "FastSIMD/InlInclude.h"
#include <tuple>

#include "DomainWarpFractal.h"

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalProgressive, FS> : public virtual FastNoise::DomainWarpFractalProgressive, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    float32v GenT(int32v seed, P... pos)
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
    float32v GenT( int32v seed, P... noisePos )
    {
        auto f = [=] ( auto&... warpPos )
        {
            float32v amp = float32v(this->mSource[0]->GetWarpAmplitude() * mFractalBounding);
            float32v freq = float32v(this->mSource[0]->GetWarpFrequency());
            int32v seedInc = seed;
        
            this->mSource[0]->Warp(seedInc, amp, (noisePos * freq)..., warpPos...);
    
            for (int i = 1; i < mOctaves; i++)
            {
                seedInc -= int32v( -1 );
                freq *= float32v( mLacunarity );
                amp *= float32v( mGain );
                this->mSource[0]->Warp(seedInc, amp, (noisePos * freq)..., warpPos...);
            }
    
            return this->mSource[0]->GetSourceSIMD()->Gen( seed, warpPos... );
        };

        return f( noisePos... );
    }
};

//template<typename T>
//typename FS_CLASS( FastNoise::DomainWarpFractalIndependant )<T>::float32v
//FS_CLASS( FastNoise::DomainWarpFractalIndependant )<T>::Gen( int32v seed, float32v x, float32v y, float32v z )
//{
//    float32v amp = float32v( mWarp->GetWarpAmplitude() );
//    float32v freq = float32v( mWarp->GetWarpFrequency() );
//    
//    float32v mWarpX = x;
//    float32v mWarpY = y;
//    float32v mWarpZ = z;
//
//    mWarp->Warp( seed, amp, (pos * freq)..., pos... );
//    
//    for ( int i = 1; i < mOctaves; i++ )
//    {
//        seed -= int32v( -1 );
//        freq *= float32v( mLacunarity );
//        amp *= float32v( mGain );
//        mWarp->Warp( seed, amp, (pos * freq)..., pos... );
//    }
//
//    return mWarp->GetSourceSIMD()->Gen( seed, pos... );
//}
