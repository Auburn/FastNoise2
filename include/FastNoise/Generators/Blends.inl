#include "Blends.h"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Add, SIMD> final : public virtual Add, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) + this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Subtract, SIMD> final : public virtual Subtract, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) - this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Multiply, SIMD> final : public virtual Multiply, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) * this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Divide, SIMD> final : public virtual Divide, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) / this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Modulus, SIMD> final : public virtual Modulus, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = this->GetSourceValue( mLHS, seed, pos... );
        float32v b = this->GetSourceValue( mRHS, seed, pos... );

        return FS::Modulus( a, b );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<PowFloat, SIMD> final : public virtual PowFloat, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v value = FS::Max( FS::Abs( this->GetSourceValue( mValue, seed, pos... ) ), float32v( FLT_MIN ) );

        return Pow( value, this->GetSourceValue( mPow, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<PowInt, SIMD> final : public virtual PowInt, public DispatchClass<Generator, SIMD>
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
class FastSIMD::DispatchClass<Min, SIMD> final : public virtual Min, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS::Min( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Max, SIMD> final : public virtual Max, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS::Max( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<MinSmooth, SIMD> final : public virtual MinSmooth, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = this->GetSourceValue( mLHS, seed, pos... );
        float32v b = this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS::Max( float32v( FLT_MIN ), FS::Abs( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<MaxSmooth, SIMD> final : public virtual MaxSmooth, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = -this->GetSourceValue( mLHS, seed, pos... );
        float32v b = -this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS::Max( float32v( FLT_MIN ), FS::Abs( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return -FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Fade, SIMD> final : public virtual Fade, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v fade = this->GetSourceValue( mFade, seed, pos... );
        float32v fadeMin = this->GetSourceValue( mFadeMin, seed, pos... );
        float32v fadeMax = this->GetSourceValue( mFadeMax, seed, pos... );

        float32v fadeRange = fadeMax - fadeMin;

        fade = ( fade - fadeMin ) / fadeRange;

        fade = FS::Max( float32v( 0 ), FS::Min( float32v( 1 ), fade ) );

        switch( mInterpolation )
        {
        case Interpolation::Linear:
            break;
        case Interpolation::Hermite:
            fade = InterpHermite( fade );
            break;
        case Interpolation::Quintic:
            fade = InterpQuintic( fade );
            break;
        }

        // Protect against nan from 0 range div
        fade = FS::Select( fadeRange == float32v( 0 ), float32v( 0.5f ), fade );
        
        return Lerp( this->GetSourceValue( mA, seed, pos... ), this->GetSourceValue( mB, seed, pos... ), fade );
    }
};

