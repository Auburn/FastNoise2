#define FASTSIMD_INTELLISENSE
#include "DomainWarpFractal.h"

#include <tuple>

template<typename F, FastSIMD::eLevel S>
void FS_CLASS( FastNoise::DomainWarpFractal )<F, S>::SetDomainWarpSource( const std::shared_ptr<FastNoise::DomainWarp>& domainWarp )
{
    mWarp = FS_CLASS( DomainWarp )<FS>::GetSIMD_DomainWarp( domainWarp.get() );

    if ( mWarp )
    {
        mWarpBase = domainWarp;
    }
}

template<typename F, FastSIMD::eLevel S>
template<typename... P>
typename FS_CLASS( FastNoise::DomainWarpFractalProgressive )<F, S>::float32v
FS_CLASS( FastNoise::DomainWarpFractalProgressive )<F, S>::GenT( int32v seed, P... pos )
{
    float32v amp = float32v( this->mWarp->GetWarpAmplitude() * mFractalBounding );
    float32v freq = float32v( this->mWarp->GetWarpFrequency() );
    int32v seedInc = seed;

    this->mWarp->Warp( seedInc, amp, (pos * freq)..., pos... );
    
    for ( int i = 1; i < mOctaves; i++ )
    {
        seedInc -= int32v( -1 );
        freq *= float32v( mLacunarity );
        amp *= float32v( mGain );
        this->mWarp->Warp( seedInc, amp, (pos * freq)..., pos... );
    }

    return this->mWarp->GetSourceSIMD()->Gen( seed, pos... );
}

template<typename F, FastSIMD::eLevel S>
template<typename... P, std::size_t... I>
typename FS_CLASS( FastNoise::DomainWarpFractalIndependant )<F, S>::float32v
FS_CLASS( FastNoise::DomainWarpFractalIndependant )<F, S>::GenT( int32v seed, std::index_sequence<I...>, P... pos )
{
    float32v amp = float32v( this->mWarp->GetWarpAmplitude() * mFractalBounding );
    float32v freq = float32v( this->mWarp->GetWarpFrequency() );
    int32v seedInc = seed;

    auto warpPos = std::make_tuple( pos... );

    this->mWarp->Warp( seedInc, amp, (pos * freq)..., std::get<I>( warpPos )... );
    
    for ( int i = 1; i < mOctaves; i++ )
    {
        seedInc -= int32v( -1 );
        freq *= float32v( mLacunarity );
        amp *= float32v( mGain );
        this->mWarp->Warp( seedInc, amp, (pos * freq)..., std::get<I>( warpPos )... );
    }

    return this->mWarp->GetSourceSIMD()->Gen( seed, std::get<I>( warpPos )... );
}

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
