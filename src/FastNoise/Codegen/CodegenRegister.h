#pragma once
// Symbolic FastSIMD register types used to trace FastNoise node code into the IR.
//
// Node implementations are written against FS::Register<T, N, SIMD>. This header
// specialises those templates for a pseudo feature set that no hardware backend uses,
// so the *unmodified* node code can be instantiated with registers that record an
// expression DAG (CodegenIR.h) instead of computing values.
//
// Only the primitives that FastSIMD cannot compose itself are implemented here;
// everything in FastSIMD/ToolSet/Generic/Functions.h that has an `IsNativeV` path
// (MaskedAdd, Increment, SignBit, Reciprocal, Modulus, ...) works unchanged on top of
// the operators below.

#include <cassert>
#include <cstdint>

#include <FastSIMD/ToolSet.h>

#include "CodegenIR.h"

namespace FastNoise::Codegen
{
    // FeatureSet::Invalid has no feature flags, so every `if constexpr( SIMD & Flag )` in node
    // code takes its generic branch, and no real dispatch backend ever instantiates
    // DispatchClass<T, Invalid>, which keeps these instantiations ODR-distinct from the SIMD ones.
    static constexpr FastSIMD::FeatureSet kFeatureSet = FastSIMD::FeatureSet::Invalid;

    class Tracer;

    // Tracing context for the thread currently running node code
    struct Context
    {
        Builder* builder = nullptr;
        Tracer* tracer = nullptr;
    };

    inline thread_local Context gContext;

    inline Builder& ActiveBuilder()
    {
        assert( gContext.builder && "Symbolic register used outside of a trace" );
        return *gContext.builder;
    }

    struct FromId {};

    template<typename T> struct TypeOf;
    template<> struct TypeOf<float> { static constexpr Type value = Type::F32; };
    template<> struct TypeOf<std::int32_t> { static constexpr Type value = Type::I32; };
    template<std::size_t N, bool B> struct TypeOf<FS::Mask<N, B>> { static constexpr Type value = Type::Bool; };

    // Common storage for all symbolic registers: an instruction index in the active function
    template<typename ELEMENT>
    struct SymbolicValue
    {
        std::int32_t id = -1; // -1 = default constructed, becomes Undef on first read

        std::int32_t Id() const
        {
            if( id < 0 )
            {
                return ActiveBuilder().Leaf( Op::Undef, TypeOf<ELEMENT>::value, 0 );
            }
            return id;
        }
    };
}

namespace FS
{
    // ----- float32v -----
    template<std::size_t N>
    struct Register<float, N, FastNoise::Codegen::kFeatureSet, void> : FastNoise::Codegen::SymbolicValue<float>
    {
        static constexpr std::size_t ElementCount = N;
        static constexpr auto FeatureFlags = FastNoise::Codegen::kFeatureSet;

        using NativeType = float; // Lets FastSIMD's generic helpers compose this type from primitives
        using ElementType = float;
        using MaskType = Register<Mask<32, true>, N, FeatureFlags>;
        using MaskTypeArg = MaskType;

        Register() = default;
        Register( float v ) { id = FastNoise::Codegen::ActiveBuilder().ConstF( v ); }
        Register( FastNoise::Codegen::FromId, std::int32_t i ) { id = i; }

        Register& Bin( FastNoise::Codegen::Op op, const Register& rhs )
        {
            id = FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::Type::F32, Id(), rhs.Id() );
            return *this;
        }

        Register& operator +=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FAdd, rhs ); }
        Register& operator -=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FSub, rhs ); }
        Register& operator *=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FMul, rhs ); }
        Register& operator /=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FDiv, rhs ); }
        Register& operator &=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FAnd, rhs ); }
        Register& operator |=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FOr, rhs ); }
        Register& operator ^=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::FXor, rhs ); }

        Register operator -() const { return Register( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::FNeg, FastNoise::Codegen::Type::F32, Id() ) ); }
        Register operator ~() const { return Register( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::FNot, FastNoise::Codegen::Type::F32, Id() ) ); }

        MaskType Cmp( FastNoise::Codegen::Op op, const Register& rhs ) const
        {
            return MaskType( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::Type::Bool, Id(), rhs.Id() ) );
        }

        MaskType operator ==( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FEq, rhs ); }
        MaskType operator !=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FNe, rhs ); }
        MaskType operator >=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FGe, rhs ); }
        MaskType operator <=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FLe, rhs ); }
        MaskType operator > ( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FGt, rhs ); }
        MaskType operator < ( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::FLt, rhs ); }
    };

    // ----- int32v -----
    template<std::size_t N>
    struct Register<std::int32_t, N, FastNoise::Codegen::kFeatureSet, void> : FastNoise::Codegen::SymbolicValue<std::int32_t>
    {
        static constexpr std::size_t ElementCount = N;
        static constexpr auto FeatureFlags = FastNoise::Codegen::kFeatureSet;

        using NativeType = std::int32_t;
        using ElementType = std::int32_t;
        using MaskType = Register<Mask<32, false>, N, FeatureFlags>;
        using MaskTypeArg = Register<Mask<32, true>, N, FeatureFlags>;

        Register() = default;
        Register( std::int32_t v ) { id = FastNoise::Codegen::ActiveBuilder().ConstI( v ); }
        Register( FastNoise::Codegen::FromId, std::int32_t i ) { id = i; }

        Register& Bin( FastNoise::Codegen::Op op, const Register& rhs )
        {
            id = FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::Type::I32, Id(), rhs.Id() );
            return *this;
        }

        Register& Shift( FastNoise::Codegen::Op op, int amount )
        {
            id = FastNoise::Codegen::ActiveBuilder().Unary( op, FastNoise::Codegen::Type::I32, Id(), amount );
            return *this;
        }

        Register& operator +=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::IAdd, rhs ); }
        Register& operator -=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::ISub, rhs ); }
        Register& operator *=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::IMul, rhs ); }
        Register& operator &=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::IAnd, rhs ); }
        Register& operator |=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::IOr, rhs ); }
        Register& operator ^=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::IXor, rhs ); }
        Register& operator >>=( int rhs ) { return Shift( FastNoise::Codegen::Op::IShrArith, rhs ); }
        Register& operator <<=( int rhs ) { return Shift( FastNoise::Codegen::Op::IShl, rhs ); }

        Register operator -() const { return Register( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::INeg, FastNoise::Codegen::Type::I32, Id() ) ); }
        Register operator ~() const { return Register( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::INot, FastNoise::Codegen::Type::I32, Id() ) ); }

        MaskType Cmp( FastNoise::Codegen::Op op, const Register& rhs ) const
        {
            return MaskType( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::Type::Bool, Id(), rhs.Id() ) );
        }

        MaskType operator ==( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::IEq, rhs ); }
        MaskType operator !=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::INe, rhs ); }
        MaskType operator >=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::IGe, rhs ); }
        MaskType operator <=( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::ILe, rhs ); }
        MaskType operator > ( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::IGt, rhs ); }
        MaskType operator < ( const Register& rhs ) const { return Cmp( FastNoise::Codegen::Op::ILt, rhs ); }
    };

    // ----- mask32v (both the float-optimised and int flavours share one representation) -----
    template<std::size_t N, bool OPTIMISE_FLOAT>
    struct Register<Mask<32, OPTIMISE_FLOAT>, N, FastNoise::Codegen::kFeatureSet, void> : FastNoise::Codegen::SymbolicValue<Mask<32, OPTIMISE_FLOAT>>
    {
        static constexpr std::size_t ElementCount = N;
        static constexpr auto FeatureFlags = FastNoise::Codegen::kFeatureSet;

        using NativeType = bool;
        using ElementType = Mask<32, OPTIMISE_FLOAT>;
        using MaskType = Register;
        using MaskTypeArg = Register<Mask<32, true>, N, FeatureFlags>;

        Register() = default;
        Register( FastNoise::Codegen::FromId, std::int32_t i ) { this->id = i; }

        // Mask<32, false> -> Mask<32, true> and back
        template<bool B>
        Register( const Register<Mask<32, B>, N, FeatureFlags>& other ) { this->id = other.id; }

        Register& Bin( FastNoise::Codegen::Op op, const Register& rhs )
        {
            this->id = FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::Type::Bool, this->Id(), rhs.Id() );
            return *this;
        }

        Register& operator &=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::BAnd, rhs ); }
        Register& operator |=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::BOr, rhs ); }
        Register& operator ^=( const Register& rhs ) { return Bin( FastNoise::Codegen::Op::BXor, rhs ); }

        Register operator ~() const { return Register( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::BNot, FastNoise::Codegen::Type::Bool, this->Id() ) ); }
    };

    // ----- FS:: primitives on the symbolic types -----
    namespace impl
    {
        template<std::size_t N> using CgF32 = Register<float, N, FastNoise::Codegen::kFeatureSet>;
        template<std::size_t N> using CgI32 = Register<std::int32_t, N, FastNoise::Codegen::kFeatureSet>;
        template<std::size_t N, bool B> using CgMask = Register<Mask<32, B>, N, FastNoise::Codegen::kFeatureSet>;

        template<typename R>
        R CgMake( std::int32_t id ) { return R( FastNoise::Codegen::FromId{}, id ); }

        template<typename R, typename... ARGS>
        R CgUnary( FastNoise::Codegen::Op op, const R& a, int imm = 0 )
        {
            return CgMake<R>( FastNoise::Codegen::ActiveBuilder().Unary( op, FastNoise::Codegen::TypeOf<typename R::ElementType>::value, a.Id(), imm ) );
        }

        template<typename R>
        R CgBinary( FastNoise::Codegen::Op op, const R& a, const R& b )
        {
            return CgMake<R>( FastNoise::Codegen::ActiveBuilder().Binary( op, FastNoise::Codegen::TypeOf<typename R::ElementType>::value, a.Id(), b.Id() ) );
        }

        template<typename R>
        R CgTernary( FastNoise::Codegen::Op op, std::int32_t a, const R& b, const R& c )
        {
            return CgMake<R>( FastNoise::Codegen::ActiveBuilder().Ternary( op, FastNoise::Codegen::TypeOf<typename R::ElementType>::value, a, b.Id(), c.Id() ) );
        }
    }

    // Float unary
    template<std::size_t N> impl::CgF32<N> Abs( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FAbs, a ); }
    template<std::size_t N> impl::CgF32<N> Floor( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FFloor, a ); }
    template<std::size_t N> impl::CgF32<N> Ceil( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FCeil, a ); }
    template<std::size_t N> impl::CgF32<N> Trunc( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FTrunc, a ); }
    template<std::size_t N> impl::CgF32<N> Round( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FRoundEven, a ); }
    template<std::size_t N> impl::CgF32<N> Sqrt( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FSqrt, a ); }
    template<std::size_t N> impl::CgF32<N> InvSqrt( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FInvSqrt, a ); }
    template<std::size_t N> impl::CgF32<N> Exp( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FExp, a ); }
    template<std::size_t N> impl::CgF32<N> Log( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FLog, a ); }
    template<std::size_t N> impl::CgF32<N> Sin( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FSin, a ); }
    template<std::size_t N> impl::CgF32<N> Cos( const impl::CgF32<N>& a ) { return impl::CgUnary( FastNoise::Codegen::Op::FCos, a ); }

    // Float binary / ternary
    template<std::size_t N> impl::CgF32<N> Min( const impl::CgF32<N>& a, const impl::CgF32<N>& b ) { return impl::CgBinary( FastNoise::Codegen::Op::FMin, a, b ); }
    template<std::size_t N> impl::CgF32<N> Max( const impl::CgF32<N>& a, const impl::CgF32<N>& b ) { return impl::CgBinary( FastNoise::Codegen::Op::FMax, a, b ); }
    template<std::size_t N> impl::CgF32<N> Pow( const impl::CgF32<N>& a, const impl::CgF32<N>& b ) { return impl::CgBinary( FastNoise::Codegen::Op::FPow, a, b ); }

    template<std::size_t N> impl::CgF32<N> FMulAdd( const impl::CgF32<N>& a, const impl::CgF32<N>& b, const impl::CgF32<N>& c ) { return impl::CgTernary( FastNoise::Codegen::Op::FMulAdd, a.Id(), b, c ); }
    template<std::size_t N> impl::CgF32<N> FMulSub( const impl::CgF32<N>& a, const impl::CgF32<N>& b, const impl::CgF32<N>& c ) { return impl::CgTernary( FastNoise::Codegen::Op::FMulSub, a.Id(), b, c ); }
    template<std::size_t N> impl::CgF32<N> FNMulAdd( const impl::CgF32<N>& a, const impl::CgF32<N>& b, const impl::CgF32<N>& c ) { return impl::CgTernary( FastNoise::Codegen::Op::FNMulAdd, a.Id(), b, c ); }
    template<std::size_t N> impl::CgF32<N> FNMulSub( const impl::CgF32<N>& a, const impl::CgF32<N>& b, const impl::CgF32<N>& c ) { return impl::CgTernary( FastNoise::Codegen::Op::FNMulSub, a.Id(), b, c ); }

    // Select / Masked (the mask parameter is a non-deduced context so Mask<32, false> converts implicitly, like FastSIMD)
    template<std::size_t N> impl::CgF32<N> Select( const typename impl::CgF32<N>::MaskTypeArg& m, const impl::CgF32<N>& a, const impl::CgF32<N>& b ) { return impl::CgTernary( FastNoise::Codegen::Op::Select, m.Id(), a, b ); }
    template<std::size_t N> impl::CgI32<N> Select( const typename impl::CgI32<N>::MaskTypeArg& m, const impl::CgI32<N>& a, const impl::CgI32<N>& b ) { return impl::CgTernary( FastNoise::Codegen::Op::Select, m.Id(), a, b ); }
    template<std::size_t N> impl::CgF32<N> Masked( const typename impl::CgF32<N>::MaskTypeArg& m, const impl::CgF32<N>& a ) { return Select( m, a, impl::CgF32<N>( 0.0f ) ); }
    template<std::size_t N> impl::CgI32<N> Masked( const typename impl::CgI32<N>::MaskTypeArg& m, const impl::CgI32<N>& a ) { return Select( m, a, impl::CgI32<N>( 0 ) ); }

    // Select on the sign bit of an int or float register
    template<std::size_t N, typename T> impl::CgF32<N> SelectHighBit( const Register<T, N, FastNoise::Codegen::kFeatureSet>& m, const impl::CgF32<N>& a, const impl::CgF32<N>& b )
    {
        return Select( Cast<std::int32_t>( m ) < impl::CgI32<N>( 0 ), a, b );
    }
    template<std::size_t N, typename T> impl::CgI32<N> SelectHighBit( const Register<T, N, FastNoise::Codegen::kFeatureSet>& m, const impl::CgI32<N>& a, const impl::CgI32<N>& b )
    {
        return Select( Cast<std::int32_t>( m ) < impl::CgI32<N>( 0 ), a, b );
    }

    // Int
    template<std::size_t N> impl::CgI32<N> BitShiftRightZeroExtend( const impl::CgI32<N>& a, int b ) { return impl::CgUnary( FastNoise::Codegen::Op::IShrLogical, a, b ); }
    template<std::size_t N> impl::CgI32<N> Min( const impl::CgI32<N>& a, const impl::CgI32<N>& b ) { return Select( a < b, a, b ); }
    template<std::size_t N> impl::CgI32<N> Max( const impl::CgI32<N>& a, const impl::CgI32<N>& b ) { return Select( a > b, a, b ); }

    // Conversions
    template<std::size_t N> impl::CgI32<N> Convert( const impl::CgF32<N>& a, TypeDummy<std::int32_t> ) { return impl::CgMake<impl::CgI32<N>>( FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::F2I, FastNoise::Codegen::Type::I32, a.Id() ) ); }
    template<std::size_t N> impl::CgF32<N> Convert( const impl::CgI32<N>& a, TypeDummy<float> ) { return impl::CgMake<impl::CgF32<N>>( FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::I2F, FastNoise::Codegen::Type::F32, a.Id() ) ); }

    template<std::size_t N> impl::CgI32<N> Cast( const impl::CgF32<N>& a, TypeDummy<std::int32_t> ) { return impl::CgMake<impl::CgI32<N>>( FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::BitcastF2I, FastNoise::Codegen::Type::I32, a.Id() ) ); }
    template<std::size_t N> impl::CgF32<N> Cast( const impl::CgI32<N>& a, TypeDummy<float> ) { return impl::CgMake<impl::CgF32<N>>( FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::BitcastI2F, FastNoise::Codegen::Type::F32, a.Id() ) ); }
    template<std::size_t N> impl::CgF32<N> Cast( const impl::CgF32<N>& a, TypeDummy<float> ) { return a; }
    template<std::size_t N> impl::CgI32<N> Cast( const impl::CgI32<N>& a, TypeDummy<std::int32_t> ) { return a; }
    template<std::size_t N, bool B> impl::CgI32<N> Cast( const impl::CgMask<N, B>& a, TypeDummy<std::int32_t> ) { return impl::CgMake<impl::CgI32<N>>( FastNoise::Codegen::ActiveBuilder().Unary( FastNoise::Codegen::Op::B2I, FastNoise::Codegen::Type::I32, a.Id() ) ); }

    // Mask
    template<std::size_t N, bool B> bool AnyMask( const impl::CgMask<N, B>& )
    {
        // Node code only uses AnyMask to skip work that is masked out anyway, so "maybe" is always safe
        return true;
    }

    // Memory access only appears in GeneratorCache's thread local cache, which has no meaning in generated code.
    // Loads produce undefined values that end up dead; stores are dropped.
    template<std::size_t N> impl::CgF32<N> Load( TypeWrapper<const float*, N, FastNoise::Codegen::kFeatureSet> ) { return impl::CgF32<N>(); }
    template<std::size_t N> impl::CgI32<N> Load( TypeWrapper<const std::int32_t*, N, FastNoise::Codegen::kFeatureSet> ) { return impl::CgI32<N>(); }
    template<std::size_t N> void Store( float*, const impl::CgF32<N>& ) {}
    template<std::size_t N> void Store( std::int32_t*, const impl::CgI32<N>& ) {}

    // Only referenced by the (never instantiated) batch loops in Generator.inl, which are
    // still type checked because float32v is not a dependent type there.
    template<std::size_t N> float Extract0( const impl::CgF32<N>& ) { return 0.0f; }
    template<std::size_t N> std::int32_t Extract0( const impl::CgI32<N>& ) { return 0; }
}
