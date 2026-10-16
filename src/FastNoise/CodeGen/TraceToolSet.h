#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <FastSIMD/DispatchClass.h>

#include "IR.h"

// Symbolic stand in for FastSIMD's ToolSet
// Node implementations compiled against these register types record the operations they perform into the CodeGen IR,
// instead of computing values. Used in place of <FastSIMD/ToolSet.h>, which must not be included in the same translation unit

#define FS_FORCEINLINE inline
#define FS_NEVERINLINE
#define FS_VECTORCALL
#define FS_BIND_INTRINSIC( FUNC ) []( auto... ARGS ){ return FUNC( ARGS... ); }

namespace FastSIMD
{
    // No feature flags are set so every SIMD specific branch in the node code takes the generic path,
    // the value doesn't match any real feature set so DispatchClass<T, kTraceFeatureSet> never collides with a real one
    static constexpr FeatureSet kTraceFeatureSet = static_cast<FeatureSet>( 1u << 31 );

    // Also without feature flags, instantiates the generic implementation of helpers that are specialised for kTraceFeatureSet
    static constexpr FeatureSet kTraceGenericFeatureSet = static_cast<FeatureSet>( 1u << 30 );

    template<FeatureSet SIMD = kTraceFeatureSet>
    static constexpr FeatureSet FeatureSetDefault()
    {
        return SIMD;
    }

    // Trace the exact paths (sqrt instead of rsqrt approximations), generated code picks its own approximations
    template<auto = 0, bool RELAXED = false>
    static constexpr bool IsRelaxed()
    {
        return RELAXED;
    }

    template<typename T, FeatureSet SIMD>
    class DispatchClass;
}

namespace FS
{
    namespace CG = FastNoise::CodeGen::IR;

    template<std::size_t N, bool OPTIMISE_FLOAT = true>
    struct Mask
    {
    };

    struct TraceIdTag
    {
    };

    // Registers are literal types (constexpr constructor from an id) like FastSIMD's, node code has constexpr lambdas taking them by value

    struct TraceMask32
    {
        static constexpr std::size_t ElementCount = 1;
        using ElementType = Mask<32>;

        CG::ValueId id;

        TraceMask32() : id( CG::Builder::Current().Undef( CG::Type::Bool ) ) { }
        constexpr TraceMask32( CG::ValueId v, TraceIdTag ) : id( v ) { }

        TraceMask32& operator&=( const TraceMask32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::BAnd, id, rhs.id ); return *this; }
        TraceMask32& operator|=( const TraceMask32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::BOr, id, rhs.id ); return *this; }
        TraceMask32& operator^=( const TraceMask32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::BXor, id, rhs.id ); return *this; }

        TraceMask32 operator~() const { return { CG::Builder::Current().Emit( CG::Op::BNot, id ), {} }; }
    };

    inline TraceMask32 operator&( TraceMask32 lhs, const TraceMask32& rhs ) { return lhs &= rhs; }
    inline TraceMask32 operator|( TraceMask32 lhs, const TraceMask32& rhs ) { return lhs |= rhs; }
    inline TraceMask32 operator^( TraceMask32 lhs, const TraceMask32& rhs ) { return lhs ^= rhs; }

    struct TraceFloat32
    {
        static constexpr std::size_t ElementCount = 1;
        using ElementType = float;
        using MaskType = TraceMask32;
        using MaskTypeArg = TraceMask32;

        CG::ValueId id;

        TraceFloat32() : id( CG::Builder::Current().Undef( CG::Type::F32 ) ) { }
        TraceFloat32( float v ) : id( CG::Builder::Current().ConstF( v ) ) { }
        constexpr TraceFloat32( CG::ValueId v, TraceIdTag ) : id( v ) { }

        TraceFloat32& operator+=( const TraceFloat32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::FAdd, id, rhs.id ); return *this; }
        TraceFloat32& operator-=( const TraceFloat32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::FSub, id, rhs.id ); return *this; }
        TraceFloat32& operator*=( const TraceFloat32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::FMul, id, rhs.id ); return *this; }
        TraceFloat32& operator/=( const TraceFloat32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::FDiv, id, rhs.id ); return *this; }
        TraceFloat32& operator&=( const TraceFloat32& rhs ) { id = BitwiseOp( CG::Op::IAnd, rhs ); return *this; }
        TraceFloat32& operator|=( const TraceFloat32& rhs ) { id = BitwiseOp( CG::Op::IOr, rhs ); return *this; }
        TraceFloat32& operator^=( const TraceFloat32& rhs ) { id = BitwiseOp( CG::Op::IXor, rhs ); return *this; }

        TraceFloat32 operator-() const { return { CG::Builder::Current().Emit( CG::Op::FNeg, id ), {} }; }
        TraceFloat32 operator~() const
        {
            CG::Builder& b = CG::Builder::Current();
            return { b.Emit( CG::Op::BitcastI2F, b.Emit( CG::Op::INot, b.Emit( CG::Op::BitcastF2I, id ) ) ), {} };
        }

        TraceMask32 operator==( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FEq, id, rhs.id ), {} }; }
        TraceMask32 operator!=( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FNe, id, rhs.id ), {} }; }
        TraceMask32 operator<( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FLt, id, rhs.id ), {} }; }
        TraceMask32 operator<=( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FLe, id, rhs.id ), {} }; }
        TraceMask32 operator>( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FGt, id, rhs.id ), {} }; }
        TraceMask32 operator>=( const TraceFloat32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::FGe, id, rhs.id ), {} }; }

    private:
        // Float bitwise ops are traced as int ops on the bit patterns
        CG::ValueId BitwiseOp( CG::Op op, const TraceFloat32& rhs ) const
        {
            CG::Builder& b = CG::Builder::Current();
            return b.Emit( CG::Op::BitcastI2F, b.Emit( op, b.Emit( CG::Op::BitcastF2I, id ), b.Emit( CG::Op::BitcastF2I, rhs.id ) ) );
        }
    };

    inline TraceFloat32 operator+( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs += rhs; }
    inline TraceFloat32 operator-( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs -= rhs; }
    inline TraceFloat32 operator*( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs *= rhs; }
    inline TraceFloat32 operator/( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs /= rhs; }
    inline TraceFloat32 operator&( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs &= rhs; }
    inline TraceFloat32 operator|( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs |= rhs; }
    inline TraceFloat32 operator^( TraceFloat32 lhs, const TraceFloat32& rhs ) { return lhs ^= rhs; }

    struct TraceInt32
    {
        static constexpr std::size_t ElementCount = 1;
        using ElementType = std::int32_t;
        using MaskType = TraceMask32;
        using MaskTypeArg = TraceMask32;

        CG::ValueId id;

        TraceInt32() : id( CG::Builder::Current().Undef( CG::Type::I32 ) ) { }
        TraceInt32( std::int32_t v ) : id( CG::Builder::Current().ConstI( v ) ) { }
        constexpr TraceInt32( CG::ValueId v, TraceIdTag ) : id( v ) { }

        TraceInt32& operator+=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::IAdd, id, rhs.id ); return *this; }
        TraceInt32& operator-=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::ISub, id, rhs.id ); return *this; }
        TraceInt32& operator*=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::IMul, id, rhs.id ); return *this; }
        TraceInt32& operator&=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::IAnd, id, rhs.id ); return *this; }
        TraceInt32& operator|=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::IOr, id, rhs.id ); return *this; }
        TraceInt32& operator^=( const TraceInt32& rhs ) { id = CG::Builder::Current().Emit( CG::Op::IXor, id, rhs.id ); return *this; }
        TraceInt32& operator<<=( int rhs ) { id = CG::Builder::Current().EmitShift( CG::Op::IShl, id, rhs ); return *this; }
        TraceInt32& operator>>=( int rhs ) { id = CG::Builder::Current().EmitShift( CG::Op::IShrA, id, rhs ); return *this; }

        TraceInt32 operator-() const { return { CG::Builder::Current().Emit( CG::Op::INeg, id ), {} }; }
        TraceInt32 operator~() const { return { CG::Builder::Current().Emit( CG::Op::INot, id ), {} }; }

        TraceMask32 operator==( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::IEq, id, rhs.id ), {} }; }
        TraceMask32 operator!=( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::INe, id, rhs.id ), {} }; }
        TraceMask32 operator<( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::ILt, id, rhs.id ), {} }; }
        TraceMask32 operator<=( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::ILe, id, rhs.id ), {} }; }
        TraceMask32 operator>( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::IGt, id, rhs.id ), {} }; }
        TraceMask32 operator>=( const TraceInt32& rhs ) const { return { CG::Builder::Current().Emit( CG::Op::IGe, id, rhs.id ), {} }; }
    };

    inline TraceInt32 operator+( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs += rhs; }
    inline TraceInt32 operator-( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs -= rhs; }
    inline TraceInt32 operator*( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs *= rhs; }
    inline TraceInt32 operator&( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs &= rhs; }
    inline TraceInt32 operator|( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs |= rhs; }
    inline TraceInt32 operator^( TraceInt32 lhs, const TraceInt32& rhs ) { return lhs ^= rhs; }
    inline TraceInt32 operator<<( TraceInt32 lhs, int rhs ) { return lhs <<= rhs; }
    inline TraceInt32 operator>>( TraceInt32 lhs, int rhs ) { return lhs >>= rhs; }

    template<typename T>
    struct TraceRegisterFor;

    template<>
    struct TraceRegisterFor<float>
    {
        using Type = TraceFloat32;
    };

    template<>
    struct TraceRegisterFor<std::int32_t>
    {
        using Type = TraceInt32;
    };

    template<typename T, std::size_t N, FastSIMD::FeatureSet SIMD = FastSIMD::kTraceFeatureSet>
    using Register = typename TraceRegisterFor<T>::Type;

    template<typename T>
    static constexpr std::size_t NativeRegisterCount( FastSIMD::FeatureSet = FastSIMD::kTraceFeatureSet )
    {
        return 1;
    }

    namespace TraceDetail
    {
        inline TraceFloat32 F( CG::ValueId v ) { return { v, {} }; }
        inline TraceInt32 I( CG::ValueId v ) { return { v, {} }; }
        inline TraceMask32 M( CG::ValueId v ) { return { v, {} }; }
        inline CG::Builder& B() { return CG::Builder::Current(); }

        inline TraceFloat32 ToFloatBits( const TraceFloat32& a ) { return a; }
        inline TraceFloat32 ToFloatBits( const TraceInt32& a ) { return F( B().Emit( CG::Op::BitcastI2F, a.id ) ); }
        inline TraceFloat32 ToFloatBits( const TraceMask32& a ) { return F( B().Emit( CG::Op::BitcastI2F, B().Emit( CG::Op::B2I, a.id ) ) ); }

        inline TraceInt32 ToIntBits( const TraceFloat32& a ) { return I( B().Emit( CG::Op::BitcastF2I, a.id ) ); }
        inline TraceInt32 ToIntBits( const TraceInt32& a ) { return a; }
        inline TraceInt32 ToIntBits( const TraceMask32& a ) { return I( B().Emit( CG::Op::B2I, a.id ) ); }

        // Masks built from register bits only appear in x86 specific branches, these use the high bit like blendv
        inline TraceMask32 ToMask( const TraceInt32& a ) { return a < TraceInt32( 0 ); }
        inline TraceMask32 ToMask( const TraceFloat32& a ) { return ToMask( ToIntBits( a ) ); }
        inline TraceMask32 ToMask( const TraceMask32& a ) { return a; }

        template<typename T>
        inline T Zero();

        template<>
        inline TraceFloat32 Zero<TraceFloat32>() { return TraceFloat32( 0.0f ); }

        template<>
        inline TraceInt32 Zero<TraceInt32>() { return TraceInt32( 0 ); }
    }

    template<typename U, typename T>
    inline auto Cast( const T& a )
    {
        if constexpr( std::is_same_v<U, float> )
        {
            return TraceDetail::ToFloatBits( a );
        }
        else if constexpr( std::is_same_v<U, std::int32_t> )
        {
            return TraceDetail::ToIntBits( a );
        }
        else
        {
            return TraceDetail::ToMask( a );
        }
    }

    template<typename U>
    inline TraceInt32 Convert( const TraceFloat32& a )
    {
        static_assert( std::is_same_v<U, std::int32_t>, "Unsupported trace conversion" );
        return TraceDetail::I( TraceDetail::B().Emit( CG::Op::F2I, a.id ) );
    }

    template<typename U>
    inline TraceFloat32 Convert( const TraceInt32& a )
    {
        static_assert( std::is_same_v<U, float>, "Unsupported trace conversion" );
        return TraceDetail::F( TraceDetail::B().Emit( CG::Op::I2F, a.id ) );
    }

    inline TraceFloat32 Select( const TraceMask32& mask, const TraceFloat32& ifTrue, const TraceFloat32& ifFalse )
    {
        return TraceDetail::F( TraceDetail::B().Emit( CG::Op::Select, mask.id, ifTrue.id, ifFalse.id ) );
    }

    inline TraceInt32 Select( const TraceMask32& mask, const TraceInt32& ifTrue, const TraceInt32& ifFalse )
    {
        return TraceDetail::I( TraceDetail::B().Emit( CG::Op::Select, mask.id, ifTrue.id, ifFalse.id ) );
    }

    template<typename M, typename T>
    inline T SelectHighBit( const M& mask, const T& ifTrue, const T& ifFalse )
    {
        return Select( TraceDetail::ToIntBits( mask ) < TraceInt32( 0 ), ifTrue, ifFalse );
    }

    inline TraceFloat32 FMulAdd( const TraceFloat32& a, const TraceFloat32& b, const TraceFloat32& c )
    {
        return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FMulAdd, a.id, b.id, c.id ) );
    }

    // Negations are exact, so these match FastSIMD's unfused (a * b) - c etc
    inline TraceFloat32 FMulSub( const TraceFloat32& a, const TraceFloat32& b, const TraceFloat32& c ) { return FMulAdd( a, b, -c ); }
    inline TraceFloat32 FNMulAdd( const TraceFloat32& a, const TraceFloat32& b, const TraceFloat32& c ) { return FMulAdd( -a, b, c ); }
    inline TraceFloat32 FNMulSub( const TraceFloat32& a, const TraceFloat32& b, const TraceFloat32& c ) { return FMulAdd( -a, b, -c ); }

    inline TraceFloat32 Min( const TraceFloat32& a, const TraceFloat32& b ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FMin, a.id, b.id ) ); }
    inline TraceFloat32 Max( const TraceFloat32& a, const TraceFloat32& b ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FMax, a.id, b.id ) ); }
    inline TraceInt32 Min( const TraceInt32& a, const TraceInt32& b ) { return TraceDetail::I( TraceDetail::B().Emit( CG::Op::IMin, a.id, b.id ) ); }
    inline TraceInt32 Max( const TraceInt32& a, const TraceInt32& b ) { return TraceDetail::I( TraceDetail::B().Emit( CG::Op::IMax, a.id, b.id ) ); }

    inline TraceFloat32 Abs( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FAbs, a.id ) ); }
    inline TraceInt32 Abs( const TraceInt32& a ) { return TraceDetail::I( TraceDetail::B().Emit( CG::Op::IAbs, a.id ) ); }

    inline TraceFloat32 Floor( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FFloor, a.id ) ); }
    inline TraceFloat32 Ceil( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FCeil, a.id ) ); }
    inline TraceFloat32 Trunc( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FTrunc, a.id ) ); }
    inline TraceFloat32 Round( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FRound, a.id ) ); }

    inline TraceFloat32 Sqrt( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FSqrt, a.id ) ); }
    inline TraceFloat32 InvSqrt( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FInvSqrt, a.id ) ); }
    inline TraceFloat32 Reciprocal( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FRcp, a.id ) ); }

    inline TraceFloat32 Sin( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FSin, a.id ) ); }
    inline TraceFloat32 Cos( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FCos, a.id ) ); }
    inline TraceFloat32 Exp( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FExp, a.id ) ); }
    inline TraceFloat32 Log( const TraceFloat32& a ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FLog, a.id ) ); }
    inline TraceFloat32 Pow( const TraceFloat32& a, const TraceFloat32& b ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FPow, a.id, b.id ) ); }
    inline TraceFloat32 Modulus( const TraceFloat32& a, const TraceFloat32& b ) { return TraceDetail::F( TraceDetail::B().Emit( CG::Op::FMod, a.id, b.id ) ); }

    inline TraceFloat32 SignBit( const TraceFloat32& a ) { return a & TraceFloat32( -0.0f ); }

    inline TraceFloat32 BitwiseAndNot( const TraceFloat32& a, const TraceFloat32& b ) { return TraceDetail::ToFloatBits( TraceDetail::I( TraceDetail::B().Emit( CG::Op::IAndNot, TraceDetail::ToIntBits( a ).id, TraceDetail::ToIntBits( b ).id ) ) ); }
    inline TraceInt32 BitwiseAndNot( const TraceInt32& a, const TraceInt32& b ) { return TraceDetail::I( TraceDetail::B().Emit( CG::Op::IAndNot, a.id, b.id ) ); }
    inline TraceMask32 BitwiseAndNot( const TraceMask32& a, const TraceMask32& b ) { return TraceDetail::M( TraceDetail::B().Emit( CG::Op::BAndNot, a.id, b.id ) ); }

    inline TraceInt32 BitShiftRightZeroExtend( const TraceInt32& a, int b ) { return TraceDetail::I( TraceDetail::B().EmitShift( CG::Op::IShrL, a.id, b ) ); }

    inline TraceInt32 Increment( const TraceInt32& a ) { return a - TraceInt32( -1 ); }
    inline TraceInt32 Decrement( const TraceInt32& a ) { return a + TraceInt32( -1 ); }
    inline TraceFloat32 Increment( const TraceFloat32& a ) { return a - TraceFloat32( -1.0f ); }
    inline TraceFloat32 Decrement( const TraceFloat32& a ) { return a + TraceFloat32( -1.0f ); }

    // Masked ops follow FastSIMD's generic definitions exactly, a + Masked( m, b ) is not a + b or a for a = -0
    template<typename T>
    inline T Masked( const TraceMask32& mask, const T& a ) { return Select( mask, a, TraceDetail::Zero<T>() ); }

    template<typename T>
    inline T InvMasked( const TraceMask32& mask, const T& a ) { return Select( mask, TraceDetail::Zero<T>(), a ); }

    template<typename T>
    inline T MaskedIncrement( const TraceMask32& mask, const T& a ) { return a - Masked( mask, T( -1 ) ); }

    template<typename T>
    inline T MaskedDecrement( const TraceMask32& mask, const T& a ) { return a + Masked( mask, T( -1 ) ); }

    template<typename T>
    inline T MaskedAdd( const TraceMask32& mask, const T& a, const T& b ) { return a + Masked( mask, b ); }

    template<typename T>
    inline T InvMaskedAdd( const TraceMask32& mask, const T& a, const T& b ) { return a + InvMasked( mask, b ); }

    template<typename T>
    inline T MaskedSub( const TraceMask32& mask, const T& a, const T& b ) { return a - Masked( mask, b ); }

    template<typename T>
    inline T InvMaskedSub( const TraceMask32& mask, const T& a, const T& b ) { return a - InvMasked( mask, b ); }

    template<typename T>
    inline T MaskedMul( const TraceMask32& mask, const T& a, const T& b ) { return Select( mask, a * b, a ); }

    template<typename T>
    inline T InvMaskedMul( const TraceMask32& mask, const T& a, const T& b ) { return Select( mask, a, a * b ); }

    // Node code only uses AnyMask to skip work that is masked out anyway, so always taking the branch is correct
    inline bool AnyMask( const TraceMask32& ) { return true; }

    // Memory access only happens in GeneratorCache and the batch functions, neither affects the traced result
    template<typename R>
    inline R Load( const typename R::ElementType* ) { return R(); }

    template<typename T, typename R>
    inline void Store( T*, const R& ) { }

    template<typename R>
    inline R LoadIncremented() { return R(); }

    inline float Extract0( const TraceFloat32& ) { return 0.0f; }

    // Only referenced in SIMD specific branches which are never taken when tracing
    template<typename T, typename... Ts>
    typename TraceRegisterFor<T>::Type Constant( T, Ts... );

    template<typename R, typename FUNC, typename... ARGS>
    R NativeExec( FUNC, const ARGS&... );
}
