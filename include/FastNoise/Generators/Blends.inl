#include "Blends.h"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Add, SIMD> final : public virtual Add, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->CombineSourceValues( mLHS, mRHS, []( float32v a, float32v b ) { return a + b; }, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // Cheap op, for sources without GenBlock the per vector path is faster
        if( !this->AnySourceHasGenBlock( mLHS, mRHS ) )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return a + b; }, seed, pos, out, count, mLHS, mRHS );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Subtract, SIMD> final : public virtual Subtract, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->CombineSourceValues( mLHS, mRHS, []( float32v a, float32v b ) { return a - b; }, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // Cheap op, for sources without GenBlock the per vector path is faster
        if( !this->AnySourceHasGenBlock( mLHS, mRHS ) )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return a - b; }, seed, pos, out, count, mLHS, mRHS );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Multiply, SIMD> final : public virtual Multiply, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        if( !mRHS.simdGeneratorPtr ) // Constant RHS path, see CombineSourceValues
        {
            float rhsConstant = mRHS.constant;

            return this->GetSourceValue( mLHS, seed, pos... ) * float32v( rhsConstant );
        }

        float32v lhs = this->GetSourceValue( mLHS, seed, pos... );

        // 0 * rhs == 0, skip evaluating RHS if LHS is 0 in every lane (e.g. masked regions)
        if( !FS::AnyMask( lhs != float32v( 0 ) ) )
        {
            return lhs;
        }

        return lhs * this->GetSourceValue( mRHS, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // Cheap op, for sources without GenBlock the per vector path is faster
        if( !this->AnySourceHasGenBlock( mLHS, mRHS ) )
        {
            return false;
        }

        if( !mRHS.simdGeneratorPtr )
        {
            this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return a * b; }, seed, pos, out, count, mLHS, mRHS );
            return true;
        }

        this->template GetSourceBlock<DIMS>( mLHS, seed, pos, out, count );

        // Same as GenT: vectors with LHS 0 in every lane are LHS. RHS is evaluated for the block if every vector needs it, else per vector
        intptr_t rhsCount = 0;
        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            rhsCount += FS::AnyMask( FS::Load<float32v>( out + i ) != float32v( 0 ) ) ? (intptr_t)int32v::ElementCount : 0;
        }

        if( rhsCount == count )
        {
            this->template ApplySourceBlock<DIMS>( mRHS, []( float32v lhs, float32v rhs ) { return lhs * rhs; }, seed, pos, out, count );
        }
        else if( rhsCount )
        {
            for( intptr_t i = 0; i < count; i += int32v::ElementCount )
            {
                float32v lhs = FS::Load<float32v>( out + i );

                if( FS::AnyMask( lhs != float32v( 0 ) ) )
                {
                    FS::Store( out + i, lhs * this->template GetSourceValueAt<DIMS>( mRHS, seed, pos, i ) );
                }
            }
        }

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Divide, SIMD> final : public virtual Divide, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        // Not CombineSourceValues: its constant RHS path compiles to lhs * ( 1 / constant ), which changes the output
        float32v lhs = this->GetSourceValue( mLHS, seed, pos... );

        return lhs / this->GetSourceValue( mRHS, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // A constant RHS is divided per vector as in GenT, in a block loop it would become a multiply by its reciprocal
        if( !mRHS.simdGeneratorPtr )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return a / b; }, seed, pos, out, count, mLHS, mRHS );

        return true;
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

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // A constant RHS is handled per vector as in GenT, see Divide
        if( !mRHS.simdGeneratorPtr )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return FS::Modulus( a, b ); }, seed, pos, out, count, mLHS, mRHS );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<PowFloat, SIMD> final : public virtual PowFloat, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->CombineSourceValues( mValue, mPow, []( float32v value, float32v pow ) { return Pow( FS::Abs( value ), pow ); }, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        this->template CombineSourceBlocks<DIMS>( []( float32v value, float32v pow ) { return Pow( FS::Abs( value ), pow ); }, seed, pos, out, count, mValue, mPow );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<PowInt, SIMD> final : public virtual PowInt, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return PowInt( this->GetSourceValue( mValue, seed, pos... ) );
    }

    FS_FORCEINLINE float32v PowInt( float32v value ) const
    {
        float32v pow = value * value;

        for( int i = 2; i < mPow; i++ )
        {
            pow *= value;
        }

        return pow;
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        if( !this->SourceHasGenBlock( mValue ) )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( [this]( float32v value ) { return PowInt( value ); }, seed, pos, out, count, mValue );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Min, SIMD> final : public virtual Min, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->CombineSourceValues( mLHS, mRHS, []( float32v a, float32v b ) { return FS::Min( a, b ); }, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // Cheap op, for sources without GenBlock the per vector path is faster
        if( !this->AnySourceHasGenBlock( mLHS, mRHS ) )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return FS::Min( a, b ); }, seed, pos, out, count, mLHS, mRHS );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Max, SIMD> final : public virtual Max, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->CombineSourceValues( mLHS, mRHS, []( float32v a, float32v b ) { return FS::Max( a, b ); }, seed, pos... );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        // Cheap op, for sources without GenBlock the per vector path is faster
        if( !this->AnySourceHasGenBlock( mLHS, mRHS ) )
        {
            return false;
        }

        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b ) { return FS::Max( a, b ); }, seed, pos, out, count, mLHS, mRHS );

        return true;
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

        return MinSmooth( a, b, this->GetSourceValue( mSmoothness, seed, pos... ) );
    }

    static FS_FORCEINLINE float32v MinSmooth( float32v a, float32v b, float32v smoothness )
    {
        smoothness = FS::Max( float32v( FLT_MIN ), FS::Abs( smoothness ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b, float32v smoothness ) { return MinSmooth( a, b, smoothness ); }, seed, pos, out, count, mLHS, mRHS, mSmoothness );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<MaxSmooth, SIMD> final : public virtual MaxSmooth, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = this->GetSourceValue( mLHS, seed, pos... );
        float32v b = this->GetSourceValue( mRHS, seed, pos... );

        return MaxSmooth( a, b, this->GetSourceValue( mSmoothness, seed, pos... ) );
    }

    static FS_FORCEINLINE float32v MaxSmooth( float32v a, float32v b, float32v smoothness )
    {
        a = -a;
        b = -b;
        smoothness = FS::Max( float32v( FLT_MIN ), FS::Abs( smoothness ) );

        float32v h = FS::Max( smoothness - FS::Abs( a - b ), float32v( 0.0f ) );

        h *= FS::Reciprocal( smoothness );

        return -FS::FNMulAdd( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS::Min( a, b ) );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        this->template CombineSourceBlocks<DIMS>( []( float32v a, float32v b, float32v smoothness ) { return MaxSmooth( a, b, smoothness ); }, seed, pos, out, count, mLHS, mRHS, mSmoothness );

        return true;
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
        float32v a = this->GetSourceValue( mA, seed, pos... );

        fade = FadeAmount( fade, fadeMin, fadeMax );

        // Lerp( a, b, 0 ) == a, skip evaluating B if no lane needs it
        if( !FS::AnyMask( fade != float32v( 0 ) ) )
        {
            return a;
        }

        return Lerp( a, this->GetSourceValue( mB, seed, pos... ), fade );
    }

    FS_FORCEINLINE float32v FadeAmount( float32v fade, float32v fadeMin, float32v fadeMax ) const
    {
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
        return FS::Select( fadeRange == float32v( 0 ), float32v( 0.5f ), fade );
    }

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FASTNOISE_NOINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        static constexpr intptr_t kBlockSize = DispatchClass<Generator, SIMD>::kGenBlockSize;
        alignas( 64 ) float fade[kBlockSize];
        alignas( 64 ) float a[kBlockSize];

        // Same as GenT: vectors with a fade amount of 0 in every lane are A. B is evaluated for the block if every vector needs it, else per vector
        this->template CombineSourceBlocks<DIMS>( [this]( float32v fadeV, float32v fadeMin, float32v fadeMax ) { return FadeAmount( fadeV, fadeMin, fadeMax ); },
            seed, pos, fade, count, mFade, mFadeMin, mFadeMax );

        this->template GetSourceBlock<DIMS>( mA, seed, pos, a, count );

        intptr_t bCount = 0;
        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            bCount += FS::AnyMask( FS::Load<float32v>( fade + i ) != float32v( 0 ) ) ? (intptr_t)int32v::ElementCount : 0;
        }

        if( bCount == count )
        {
            this->template GetSourceBlock<DIMS>( mB, seed, pos, out, count );

            for( intptr_t i = 0; i < count; i += int32v::ElementCount )
            {
                FS::Store( out + i, Lerp( FS::Load<float32v>( a + i ), FS::Load<float32v>( out + i ), FS::Load<float32v>( fade + i ) ) );
            }
            return true;
        }

        for( intptr_t i = 0; i < count; i += int32v::ElementCount )
        {
            float32v fadeV = FS::Load<float32v>( fade + i );
            float32v aV = FS::Load<float32v>( a + i );

            if( FS::AnyMask( fadeV != float32v( 0 ) ) )
            {
                aV = Lerp( aV, this->template GetSourceValueAt<DIMS>( mB, seed, pos, i ), fadeV );
            }
            FS::Store( out + i, aV );
        }

        return true;
    }
};

