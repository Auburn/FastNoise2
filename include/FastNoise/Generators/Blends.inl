#include "Blends.h"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Add, SIMD> final : public virtual FastNoise::Add, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) + this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Subtract, SIMD> final : public virtual FastNoise::Subtract, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) - this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Multiply, SIMD> final : public virtual FastNoise::Multiply, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) * this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Divide, SIMD> final : public virtual FastNoise::Divide, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) / this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::PowFloat, SIMD> final : public virtual FastNoise::PowFloat, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return Pow( this->GetSourceValue( mValue, seed, pos... ), this->GetSourceValue( mPow, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::PowInt, SIMD> final : public virtual FastNoise::PowInt, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v value = this->GetSourceValue( mValue, seed, pos... );
        float32v pow = value * value;

        for( int i = 2; i < mPow; i++ )
        {
            pow *= value;
        }

        return pow;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Min, SIMD> final : public virtual FastNoise::Min, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS::Min( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Max, SIMD> final : public virtual FastNoise::Max, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS::Max( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::MinSmooth, SIMD> final : public virtual FastNoise::MinSmooth, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = this->GetSourceValue( mLHS, seed, pos... );
        float32v b = this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS::Max( float32v( 1.175494351e-38f ), FS::Abs( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::MaxSmooth, SIMD> final : public virtual FastNoise::MaxSmooth, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = -this->GetSourceValue( mLHS, seed, pos... );
        float32v b = -this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS::Max( float32v( 1.175494351e-38f ), FS::Abs( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return -FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Fade, SIMD> final : public virtual FastNoise::Fade, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v fade = FS::Abs( this->GetSourceValue( mFade, seed, pos... ) );

        return FS::FMulAdd( this->GetSourceValue( mA, seed, pos... ), float32v( 1 ) - fade, this->GetSourceValue( mB, seed, pos... ) * fade );
    }
};

