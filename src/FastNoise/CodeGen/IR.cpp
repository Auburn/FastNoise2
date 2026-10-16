#include <cassert>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>

#include "IR.h"

using namespace FastNoise::CodeGen::IR;

bool Instr::operator==( const Instr& rhs ) const
{
    return op == rhs.op && type == rhs.type && argCount == rhs.argCount && imm == rhs.imm &&
        std::equal( args, args + argCount, rhs.args );
}

size_t Builder::InstrHash::operator()( const Instr& instr ) const
{
    size_t hash = (size_t)instr.op * 31 + (size_t)instr.type;
    hash = hash * 0x9E3779B1 + (uint32_t)instr.imm;

    for( int i = 0; i < instr.argCount; i++ )
    {
        hash = hash * 0x9E3779B1 + (uint32_t)instr.args[i];
    }
    return hash;
}

const char* FastNoise::CodeGen::IR::GetOpName( Op op )
{
    static const char* kNames[] = {
        "Param", "Const", "Undef",
        "FAdd", "FSub", "FMul", "FDiv", "FMin", "FMax", "FMulAdd", "FNeg", "FAbs", "FFloor", "FCeil", "FTrunc", "FRound",
        "FSqrt", "FInvSqrt", "FRcp", "FSin", "FCos", "FExp", "FLog", "FPow", "FMod",
        "IAdd", "ISub", "IMul", "IMin", "IMax", "IAnd", "IOr", "IXor", "IAndNot", "INeg", "INot", "IAbs", "IShl", "IShrA", "IShrL",
        "FEq", "FNe", "FLt", "FLe", "FGt", "FGe", "IEq", "INe", "ILt", "ILe", "IGt", "IGe",
        "BAnd", "BOr", "BXor", "BAndNot", "BNot",
        "Select",
        "F2I", "I2F", "BitcastF2I", "BitcastI2F", "B2I",
        "Call",
    };
    static_assert( sizeof( kNames ) / sizeof( *kNames ) == (size_t)Op::Count, "Op name missing" );

    return kNames[(size_t)op];
}

bool FastNoise::CodeGen::IR::IsCommutative( Op op )
{
    switch( op )
    {
    case Op::FAdd:
    case Op::FMul:
    case Op::IAdd:
    case Op::IMul:
    case Op::IMin:
    case Op::IMax:
    case Op::IAnd:
    case Op::IOr:
    case Op::IXor:
    case Op::FEq:
    case Op::FNe:
    case Op::IEq:
    case Op::INe:
    case Op::BAnd:
    case Op::BOr:
    case Op::BXor:
        return true;
    default:
        // FMin/FMax return the second operand for NaN and +-0, so they are not commutative
        return false;
    }
}

static Type ResultType( Op op, const Instr& a, const Instr* b )
{
    switch( op )
    {
    case Op::FEq: case Op::FNe: case Op::FLt: case Op::FLe: case Op::FGt: case Op::FGe:
    case Op::IEq: case Op::INe: case Op::ILt: case Op::ILe: case Op::IGt: case Op::IGe:
    case Op::BAnd: case Op::BOr: case Op::BXor: case Op::BAndNot: case Op::BNot:
        return Type::Bool;

    case Op::IAdd: case Op::ISub: case Op::IMul: case Op::IMin: case Op::IMax:
    case Op::IAnd: case Op::IOr: case Op::IXor: case Op::IAndNot: case Op::INeg: case Op::INot: case Op::IAbs:
    case Op::IShl: case Op::IShrA: case Op::IShrL:
    case Op::F2I: case Op::BitcastF2I: case Op::B2I:
        return Type::I32;

    case Op::Select:
        return b->type;

    default:
        (void)a;
        return Type::F32;
    }
}

// Every op must round separately like FastSIMD's non relaxed mode, this file is also built with -ffp-contract=off
static float MulAddUnfused( float a, float b, float c )
{
    volatile float mul = a * b;
    return mul + c;
}

static int32_t FloatBits( float f )
{
    int32_t i;
    std::memcpy( &i, &f, sizeof( float ) );
    return i;
}

static float BitsFloat( int32_t i )
{
    float f;
    std::memcpy( &f, &i, sizeof( float ) );
    return f;
}

static float FsMin( float a, float b ) { return b < a ? b : a; }
static float FsMax( float a, float b ) { return a < b ? b : a; }

// cvtps2dq: out of range and NaN give the "integer indefinite" value
static int32_t FsConvertToInt( float f )
{
    float rounded = std::nearbyint( f );
    if( rounded >= -2147483648.0f && rounded < 2147483648.0f )
    {
        return (int32_t)rounded;
    }
    return INT32_MIN;
}

// Scalar ports of FastSIMD's generic Sin/Cos (ToolSet/Generic/Trig.h) and Exp/Log/Pow (ToolSet/Generic/Functions.h)
static float FsTrigReduce( float x, bool& greaterThanHalfPi )
{
    float quotient = x * (float)0.1591549430918953;
    float offset = BitsFloat( ( FloatBits( x ) & INT32_MIN ) | FloatBits( 0.5f ) );
    quotient = std::trunc( quotient + offset );
    float y = MulAddUnfused( quotient, -(float)6.2831853071795862, x );

    float yAbs = std::abs( y );
    int32_t ySign = FloatBits( y ) ^ FloatBits( yAbs );
    greaterThanHalfPi = yAbs > (float)1.5707963267948966;

    yAbs = greaterThanHalfPi ? (float)3.1415926535897931 - yAbs : yAbs;
    return BitsFloat( ySign ^ FloatBits( yAbs ) );
}

static float FsSin( float x )
{
    bool greaterThanHalfPi;
    float y = FsTrigReduce( x, greaterThanHalfPi );
    float ySqr = y * y;
    float poly = (float)-2.3828544692960918e-08;
    poly = MulAddUnfused( poly, ySqr, (float)+2.7521557770526783e-06 );
    poly = MulAddUnfused( poly, ySqr, (float)-1.9840782426250314e-04 );
    poly = MulAddUnfused( poly, ySqr, (float)+8.3333303183525942e-03 );
    poly = MulAddUnfused( poly, ySqr, (float)-1.6666666601721269e-01 );
    poly = MulAddUnfused( poly, ySqr, (float)+1.0 );
    return poly * y;
}

static float FsCos( float x )
{
    bool greaterThanHalfPi;
    float y = FsTrigReduce( x, greaterThanHalfPi );
    float ySqr = y * y;
    float poly = (float)-2.6051615464872668e-07;
    poly = MulAddUnfused( poly, ySqr, (float)+2.4760495088926859e-05 );
    poly = MulAddUnfused( poly, ySqr, (float)-1.3888377661039897e-03 );
    poly = MulAddUnfused( poly, ySqr, (float)+4.1666638865338612e-02 );
    poly = MulAddUnfused( poly, ySqr, (float)-4.9999999508695869e-01 );
    poly = MulAddUnfused( poly, ySqr, (float)+1.0 );
    return ( greaterThanHalfPi ? -1.0f : 1.0f ) * poly;
}

static float FsExp( float a )
{
    float value = FsMax( FsMin( a, 88.3762626647949f ), -88.3762626647949f );

    float fx = value * 1.44269504088896341f;
    fx += 0.5f;

    float floored = std::floor( fx );
    fx = floored + ( floored > fx ? -1.0f : 0.0f );

    value -= fx * 0.693359375f;
    value -= fx * -2.12194440e-4f;

    float y = 1.9875691500E-4f;
    y *= value;
    y += 1.3981999507E-3f;
    y *= value;
    y += 8.3334519073E-3f;
    y *= value;
    y += 4.1665795894E-2f;
    y *= value;
    y += 1.6666665459E-1f;
    y *= value;
    y += 5.0000001201E-1f;
    y *= value * value;
    y += value + 1.0f;

    uint32_t i = (uint32_t)FsConvertToInt( fx );
    i += 0x7f;
    i <<= 23;

    return y * BitsFloat( (int32_t)i );
}

static float FsLog( float a )
{
    bool valid = a > 0;

    float value = FsMax( a, BitsFloat( 0x00800000 ) );
    int32_t i = (int32_t)( (uint32_t)FloatBits( value ) >> 23 );

    value = BitsFloat( ( FloatBits( value ) & ~0x7f800000 ) | FloatBits( 0.5f ) );

    i -= 0x7f;
    float e = (float)i;
    e += 1.0f;

    bool mask = value < 0.707106781186547524f;
    value = value + ( mask ? value : 0.0f );
    value -= 1.0f;
    e = e + ( mask ? -1.0f : 0.0f );

    float y = 7.0376836292E-2f;
    y *= value;
    y += -1.1514610310E-1f;
    y *= value;
    y += 1.1676998740E-1f;
    y *= value;
    y += -1.2420140846E-1f;
    y *= value;
    y += 1.4249322787E-1f;
    y *= value;
    y += -1.6668057665E-1f;
    y *= value;
    y += 2.0000714765E-1f;
    y *= value;
    y += -2.4999993993E-1f;
    y *= value;
    y += 3.3333331174E-1f;
    y *= value;

    float xx = value * value;
    y *= xx;
    y += e * -2.12194440e-4f;
    y -= xx * 0.5f;

    value += y;
    value += e * 0.693359375f;

    return valid ? value : 0.0f;
}

static float FsPow( float value, float pow )
{
    return value > 0 ? FsExp( pow * FsLog( value ) ) : 0.0f;
}

Scalar FastNoise::CodeGen::IR::EvaluateOp( Op op, Type argType, const Scalar* a, int32_t imm )
{
    (void)argType;
    Scalar r;
    r.i = 0;

    auto f = [&r]( float v ) { r.f = v; return r; };
    auto i = [&r]( int32_t v ) { r.i = v; return r; };
    auto b = [&r]( bool v ) { r.i = v ? 1 : 0; return r; };
    auto u = []( int32_t v ) { return (uint32_t)v; };

    switch( op )
    {
    case Op::FAdd: return f( a[0].f + a[1].f );
    case Op::FSub: return f( a[0].f - a[1].f );
    case Op::FMul: return f( a[0].f * a[1].f );
    case Op::FDiv: return f( a[0].f / a[1].f );
    case Op::FMin: return f( FsMin( a[0].f, a[1].f ) );
    case Op::FMax: return f( FsMax( a[0].f, a[1].f ) );
    case Op::FMulAdd: return f( MulAddUnfused( a[0].f, a[1].f, a[2].f ) );
    case Op::FNeg: return i( (int32_t)( u( a[0].i ) ^ 0x80000000u ) );
    case Op::FAbs: return i( a[0].i & 0x7FFFFFFF );
    case Op::FFloor: return f( std::floor( a[0].f ) );
    case Op::FCeil: return f( std::ceil( a[0].f ) );
    case Op::FTrunc: return f( std::trunc( a[0].f ) );
    case Op::FRound: return f( std::nearbyint( a[0].f ) );
    case Op::FSqrt: return f( std::sqrt( a[0].f ) );
    case Op::FInvSqrt: return f( 1.0f / std::sqrt( a[0].f ) );
    case Op::FRcp: return f( 1.0f / a[0].f );
    case Op::FSin: return f( FsSin( a[0].f ) );
    case Op::FCos: return f( FsCos( a[0].f ) );
    case Op::FExp: return f( FsExp( a[0].f ) );
    case Op::FLog: return f( FsLog( a[0].f ) );
    case Op::FPow: return f( FsPow( a[0].f, a[1].f ) );
    case Op::FMod:
    {
        volatile float truncMul = std::trunc( a[0].f / a[1].f ) * a[1].f;
        return f( a[0].f - truncMul );
    }

    case Op::IAnd: return i( a[0].i & a[1].i );
    case Op::IOr: return i( a[0].i | a[1].i );
    case Op::IXor: return i( a[0].i ^ a[1].i );
    case Op::IAndNot: return i( a[0].i & ~a[1].i );
    case Op::INot: return i( ~a[0].i );

    case Op::IAdd: return i( (int32_t)( u( a[0].i ) + u( a[1].i ) ) );
    case Op::ISub: return i( (int32_t)( u( a[0].i ) - u( a[1].i ) ) );
    case Op::IMul: return i( (int32_t)( u( a[0].i ) * u( a[1].i ) ) );
    case Op::IMin: return i( std::min( a[0].i, a[1].i ) );
    case Op::IMax: return i( std::max( a[0].i, a[1].i ) );
    case Op::INeg: return i( (int32_t)( 0u - u( a[0].i ) ) );
    case Op::IAbs: return i( a[0].i < 0 ? (int32_t)( 0u - u( a[0].i ) ) : a[0].i );
    case Op::IShl: return i( (int32_t)( u( a[0].i ) << imm ) );
    case Op::IShrA: return i( a[0].i >> imm );
    case Op::IShrL: return i( (int32_t)( u( a[0].i ) >> imm ) );

    case Op::FEq: return b( a[0].f == a[1].f );
    case Op::FNe: return b( a[0].f != a[1].f );
    case Op::FLt: return b( a[0].f < a[1].f );
    case Op::FLe: return b( a[0].f <= a[1].f );
    case Op::FGt: return b( a[0].f > a[1].f );
    case Op::FGe: return b( a[0].f >= a[1].f );
    case Op::IEq: return b( a[0].i == a[1].i );
    case Op::INe: return b( a[0].i != a[1].i );
    case Op::ILt: return b( a[0].i < a[1].i );
    case Op::ILe: return b( a[0].i <= a[1].i );
    case Op::IGt: return b( a[0].i > a[1].i );
    case Op::IGe: return b( a[0].i >= a[1].i );

    case Op::BAnd: return b( a[0].i && a[1].i );
    case Op::BOr: return b( a[0].i || a[1].i );
    case Op::BXor: return b( (bool)a[0].i != (bool)a[1].i );
    case Op::BAndNot: return b( a[0].i && !a[1].i );
    case Op::BNot: return b( !a[0].i );

    case Op::Select: return a[0].i ? a[1] : a[2];

    case Op::F2I: return i( FsConvertToInt( a[0].f ) );
    case Op::I2F: return f( (float)a[0].i );
    case Op::BitcastF2I: case Op::BitcastI2F: return a[0];
    case Op::B2I: return i( a[0].i ? -1 : 0 );

    default:
        assert( 0 ); // Not a pure scalar op
        return r;
    }
}

static thread_local Builder* tCurrentBuilder = nullptr;

Builder::Builder( Function& function, bool fastMath ) :
    mFunction( function ), mFastMath( fastMath ), mPrevious( tCurrentBuilder )
{
    for( size_t i = 0; i < function.code.size(); i++ )
    {
        mCse.emplace( function.code[i], (ValueId)i );
    }
    tCurrentBuilder = this;
}

Builder::~Builder()
{
    tCurrentBuilder = mPrevious;
}

Builder& Builder::Current()
{
    assert( tCurrentBuilder ); // Trace types used outside of tracing
    return *tCurrentBuilder;
}

ValueId Builder::Param( int index, Type type )
{
    Instr instr;
    instr.op = Op::Param;
    instr.type = type;
    instr.imm = index;
    return Add( instr );
}

ValueId Builder::ConstF( float value )
{
    Instr instr;
    instr.op = Op::Const;
    instr.type = Type::F32;
    std::memcpy( &instr.imm, &value, sizeof( float ) );
    return Add( instr );
}

ValueId Builder::ConstI( int32_t value )
{
    Instr instr;
    instr.op = Op::Const;
    instr.type = Type::I32;
    instr.imm = value;
    return Add( instr );
}

ValueId Builder::ConstB( bool value )
{
    Instr instr;
    instr.op = Op::Const;
    instr.type = Type::Bool;
    instr.imm = value ? 1 : 0;
    return Add( instr );
}

ValueId Builder::Undef( Type type )
{
    Instr instr;
    instr.op = Op::Undef;
    instr.type = type;
    return Add( instr );
}

ValueId Builder::Emit( Op op, ValueId a )
{
    Instr instr;
    instr.op = op;
    instr.argCount = 1;
    instr.args[0] = a;
    instr.type = ResultType( op, Get( a ), nullptr );
    return Add( instr );
}

ValueId Builder::Emit( Op op, ValueId a, ValueId b )
{
    Instr instr;
    instr.op = op;
    instr.argCount = 2;
    instr.args[0] = a;
    instr.args[1] = b;
    instr.type = ResultType( op, Get( a ), &Get( b ) );
    return Add( instr );
}

ValueId Builder::Emit( Op op, ValueId a, ValueId b, ValueId c )
{
    Instr instr;
    instr.op = op;
    instr.argCount = 3;
    instr.args[0] = a;
    instr.args[1] = b;
    instr.args[2] = c;
    instr.type = ResultType( op, Get( a ), &Get( b ) );
    return Add( instr );
}

ValueId Builder::EmitShift( Op op, ValueId a, int32_t amount )
{
    Instr instr;
    instr.op = op;
    instr.argCount = 1;
    instr.args[0] = a;
    instr.imm = amount & 31;
    instr.type = Type::I32;
    return Add( instr );
}

ValueId Builder::EmitCall( int function, const ValueId* args, int argCount )
{
    assert( argCount <= kMaxArgs );
    Instr instr;
    instr.op = Op::Call;
    instr.type = Type::F32;
    instr.imm = function;
    instr.argCount = (uint8_t)argCount;
    std::copy( args, args + argCount, instr.args );
    return Add( instr );
}

bool Builder::IsConst( ValueId v ) const
{
    return Get( v ).op == Op::Const;
}

bool Builder::IsConstF( ValueId v, float value ) const
{
    int32_t bits;
    std::memcpy( &bits, &value, sizeof( float ) );
    return Get( v ).op == Op::Const && Get( v ).imm == bits;
}

bool Builder::IsConstI( ValueId v, int32_t value ) const
{
    return Get( v ).op == Op::Const && Get( v ).imm == value;
}

static constexpr float kInfinity = std::numeric_limits<float>::infinity();

ValueId Builder::Simplify( Instr& instr )
{
    ValueId* arg = instr.args;

    // Constants to the right so emitted code reads "x * 0.5", operands ordered for CSE
    if( IsCommutative( instr.op ) && ( IsConst( arg[0] ) > IsConst( arg[1] ) || ( IsConst( arg[0] ) == IsConst( arg[1] ) && arg[0] > arg[1] ) ) )
    {
        std::swap( arg[0], arg[1] );
    }
    if( instr.op == Op::FMulAdd && IsConst( arg[0] ) && !IsConst( arg[1] ) )
    {
        std::swap( arg[0], arg[1] );
    }

    // Exact identities, only -0 handling differs where noted as fast math
    switch( instr.op )
    {
    case Op::FAdd:
        if( IsConstF( arg[1], -0.0f ) || ( mFastMath && IsConstF( arg[1], 0.0f ) ) ) return arg[0];
        break;
    case Op::FSub:
        if( IsConstF( arg[1], 0.0f ) || ( mFastMath && IsConstF( arg[1], -0.0f ) ) ) return arg[0];
        break;
    case Op::FMul:
        if( IsConstF( arg[1], 1.0f ) ) return arg[0];
        if( mFastMath && IsConstF( arg[1], 0.0f ) ) return arg[1];
        break;
    case Op::FDiv:
        if( IsConstF( arg[1], 1.0f ) ) return arg[0];
        break;
    case Op::FMulAdd:
        if( IsConstF( arg[1], 1.0f ) ) return Emit( Op::FAdd, arg[0], arg[2] );
        if( IsConst( arg[0] ) && IsConst( arg[1] ) ) return Emit( Op::FAdd, Emit( Op::FMul, arg[0], arg[1] ), arg[2] );
        if( IsConstF( arg[2], -0.0f ) || ( mFastMath && IsConstF( arg[2], 0.0f ) ) ) return Emit( Op::FMul, arg[0], arg[1] );
        if( mFastMath && IsConstF( arg[1], 0.0f ) ) return arg[2];
        break;
    // Cellular nodes start their distances at infinity, removing it keeps generated code free of infinite literals
    case Op::FMin:
        if( arg[0] == arg[1] || IsConstF( arg[1], kInfinity ) ) return arg[0];
        if( IsConstF( arg[0], -kInfinity ) ) return arg[0];
        if( mFastMath && IsConstF( arg[0], kInfinity ) ) return arg[1];
        if( mFastMath && IsConstF( arg[1], -kInfinity ) ) return arg[1];
        break;
    case Op::FMax:
        if( arg[0] == arg[1] || IsConstF( arg[1], -kInfinity ) ) return arg[0];
        if( IsConstF( arg[0], kInfinity ) ) return arg[0];
        if( mFastMath && IsConstF( arg[0], -kInfinity ) ) return arg[1];
        if( mFastMath && IsConstF( arg[1], kInfinity ) ) return arg[1];
        break;
    case Op::FLt:
    case Op::FGt:
    {
        // a < b, normalised so lo < hi
        ValueId lo = arg[instr.op == Op::FGt];
        ValueId hi = arg[instr.op == Op::FLt];
        if( IsConstF( lo, kInfinity ) || IsConstF( hi, -kInfinity ) ) return ConstB( false );
        if( mFastMath && ( IsConstF( hi, kInfinity ) || IsConstF( lo, -kInfinity ) ) ) return ConstB( true );
        break;
    }
    case Op::IMin:
    case Op::IMax:
        if( arg[0] == arg[1] ) return arg[0];
        break;
    case Op::FNeg:
    case Op::INeg:
    case Op::INot:
    case Op::BNot:
        if( Get( arg[0] ).op == instr.op ) return Get( arg[0] ).args[0];
        break;
    case Op::BitcastF2I:
        if( Get( arg[0] ).op == Op::BitcastI2F ) return Get( arg[0] ).args[0];

        // Bit patterns are selected as ints, fast math compilers (and GPUs) don't preserve signed zero through a float select of +-0
        if( Get( arg[0] ).op == Op::Select )
        {
            Instr select = Get( arg[0] );
            ValueId ifTrue = Emit( Op::BitcastF2I, select.args[1] );
            ValueId ifFalse = Emit( Op::BitcastF2I, select.args[2] );
            return Emit( Op::Select, select.args[0], ifTrue, ifFalse );
        }
        break;
    case Op::BitcastI2F:
        if( Get( arg[0] ).op == Op::BitcastF2I ) return Get( arg[0] ).args[0];
        break;
    case Op::FAbs:
        if( Get( arg[0] ).op == Op::FAbs ) return arg[0];
        if( Get( arg[0] ).op == Op::FNeg ) return Emit( Op::FAbs, Get( arg[0] ).args[0] );
        break;
    case Op::IAdd:
    case Op::ISub:
    case Op::IOr:
    case Op::IXor:
        if( IsConstI( arg[1], 0 ) ) return arg[0];
        break;
    case Op::IMul:
        if( IsConstI( arg[1], 1 ) ) return arg[0];
        if( IsConstI( arg[1], 0 ) ) return arg[1];
        break;
    case Op::IAnd:
        if( IsConstI( arg[1], -1 ) ) return arg[0];
        if( IsConstI( arg[1], 0 ) ) return arg[1];
        break;
    case Op::IShl:
    case Op::IShrA:
    case Op::IShrL:
        if( instr.imm == 0 ) return arg[0];
        break;
    case Op::BAnd:
        if( IsConstI( arg[1], 1 ) ) return arg[0];
        if( IsConstI( arg[1], 0 ) ) return arg[1];
        if( arg[0] == arg[1] ) return arg[0];
        break;
    case Op::BOr:
        if( IsConstI( arg[1], 0 ) ) return arg[0];
        if( IsConstI( arg[1], 1 ) ) return arg[1];
        if( arg[0] == arg[1] ) return arg[0];
        break;
    case Op::BXor:
        if( IsConstI( arg[1], 0 ) ) return arg[0];
        if( IsConstI( arg[1], 1 ) ) return Emit( Op::BNot, arg[0] );
        break;
    case Op::BAndNot:
        if( IsConstI( arg[1], 0 ) ) return arg[0];
        if( IsConstI( arg[1], 1 ) || IsConstI( arg[0], 0 ) ) return ConstB( false );
        if( IsConstI( arg[0], 1 ) ) return Emit( Op::BNot, arg[1] );
        break;
    case Op::IAndNot:
        if( IsConstI( arg[1], 0 ) ) return arg[0];
        if( IsConstI( arg[1], -1 ) || IsConstI( arg[0], 0 ) ) return ConstI( 0 );
        break;
    case Op::Select:
        if( IsConst( arg[0] ) ) return Get( arg[0] ).imm ? arg[1] : arg[2];
        if( arg[1] == arg[2] ) return arg[1];
        if( instr.type == Type::Bool && IsConstI( arg[1], 1 ) && IsConstI( arg[2], 0 ) ) return arg[0];
        if( instr.type == Type::Bool && IsConstI( arg[1], 0 ) && IsConstI( arg[2], 1 ) ) return Emit( Op::BNot, arg[0] );
        break;
    default:
        break;
    }

    return kNoValue;
}

ValueId Builder::Add( Instr instr )
{
    if( instr.op != Op::Call )
    {
        // Reading an uninitialised register gives an arbitrary value, picking the other operand of a select is valid
        if( instr.op == Op::Select )
        {
            if( Get( instr.args[1] ).op == Op::Undef ) return instr.args[2];
            if( Get( instr.args[2] ).op == Op::Undef ) return instr.args[1];
        }

        // Elsewhere uninitialised registers are treated as zero
        for( int i = 0; i < instr.argCount; i++ )
        {
            const Instr& arg = Get( instr.args[i] );
            if( arg.op == Op::Undef )
            {
                instr.args[i] = arg.type == Type::F32 ? ConstF( 0 ) : arg.type == Type::I32 ? ConstI( 0 ) : ConstB( false );
            }
        }

        if( instr.argCount )
        {
            ValueId simplified = Simplify( instr );
            if( simplified != kNoValue )
            {
                return simplified;
            }

            bool allConst = true;
            Scalar args[kMaxArgs];
            for( int i = 0; i < instr.argCount; i++ )
            {
                allConst &= IsConst( instr.args[i] );
                args[i].i = Get( instr.args[i] ).imm;
            }

            if( allConst )
            {
                Scalar result = EvaluateOp( instr.op, Get( instr.args[0] ).type, args, instr.imm );

                instr.op = Op::Const;
                instr.imm = result.i;
                instr.argCount = 0;
                std::fill( instr.args, instr.args + kMaxArgs, kNoValue );
            }
        }
    }

    auto find = mCse.find( instr );
    if( find != mCse.end() )
    {
        return find->second;
    }

    ValueId id = (ValueId)mFunction.code.size();
    mFunction.code.push_back( instr );
    mCse.emplace( instr, id );
    return id;
}

std::vector<bool> FastNoise::CodeGen::IR::FindLive( const Function& function )
{
    std::vector<bool> live( function.code.size(), false );

    if( function.result != kNoValue )
    {
        live[function.result] = true;
    }

    for( size_t i = function.code.size(); i-- > 0; )
    {
        if( live[i] )
        {
            const Instr& instr = function.code[i];
            for( int a = 0; a < instr.argCount; a++ )
            {
                live[instr.args[a]] = true;
            }
        }
    }
    return live;
}

size_t FastNoise::CodeGen::IR::CountOps( const Function& function )
{
    std::vector<bool> live = FindLive( function );
    size_t count = 0;

    for( size_t i = 0; i < function.code.size(); i++ )
    {
        Op op = function.code[i].op;
        count += live[i] && op != Op::Param && op != Op::Const && op != Op::Undef;
    }
    return count;
}

void FastNoise::CodeGen::IR::Compact( Program& program )
{
    std::vector<std::vector<bool>> live( program.functions.size() );
    std::vector<int> remapFunction( program.functions.size(), -1 );
    std::vector<int> order;

    // Breadth first from the entry point, unreachable functions are dropped
    remapFunction[0] = 0;
    order.push_back( 0 );

    for( size_t o = 0; o < order.size(); o++ )
    {
        int functionIdx = order[o];
        const Function& function = program.functions[functionIdx];
        live[functionIdx] = FindLive( function );

        for( size_t i = 0; i < function.code.size(); i++ )
        {
            const Instr& instr = function.code[i];
            if( live[functionIdx][i] && instr.op == Op::Call && remapFunction[instr.imm] < 0 )
            {
                remapFunction[instr.imm] = (int)order.size();
                order.push_back( instr.imm );
            }
        }
    }

    std::vector<Function> compacted( order.size() );

    for( size_t o = 0; o < order.size(); o++ )
    {
        Function& source = program.functions[order[o]];
        Function& dest = compacted[o];
        std::vector<ValueId> remapValue( source.code.size(), kNoValue );

        dest.name = std::move( source.name );
        dest.native = std::move( source.native );
        dest.params = std::move( source.params );

        for( size_t i = 0; i < source.code.size(); i++ )
        {
            if( !live[order[o]][i] )
            {
                continue;
            }

            Instr instr = source.code[i];
            for( int a = 0; a < instr.argCount; a++ )
            {
                instr.args[a] = remapValue[instr.args[a]];
            }
            if( instr.op == Op::Call )
            {
                instr.imm = remapFunction[instr.imm];
            }

            remapValue[i] = (ValueId)dest.code.size();
            dest.code.push_back( instr );
        }

        dest.result = remapValue[source.result];
    }

    program.functions = std::move( compacted );
}

static Scalar EvaluateFunction( const Program& program, int functionIdx, const Scalar* params )
{
    const Function& function = program.functions[functionIdx];
    std::vector<Scalar> values( function.code.size() );

    for( size_t i = 0; i < function.code.size(); i++ )
    {
        const Instr& instr = function.code[i];
        Scalar args[kMaxArgs];

        for( int a = 0; a < instr.argCount; a++ )
        {
            args[a] = values[instr.args[a]];
        }

        switch( instr.op )
        {
        case Op::Param:
            values[i] = params[instr.imm];
            break;
        case Op::Const:
            values[i].i = instr.imm;
            break;
        case Op::Undef:
            values[i].i = 0;
            break;
        case Op::Call:
            values[i] = EvaluateFunction( program, instr.imm, args );
            break;
        default:
            values[i] = EvaluateOp( instr.op, instr.argCount ? function.code[instr.args[0]].type : instr.type, args, instr.imm );
            break;
        }
    }

    return values[function.result];
}

float FastNoise::CodeGen::IR::Evaluate( const Program& program, int32_t seed, const float* positions )
{
    Scalar params[kMaxArgs];
    params[0].i = seed;

    for( size_t i = 1; i < program.functions[0].params.size(); i++ )
    {
        params[i].f = positions[i - 1];
    }

    return EvaluateFunction( program, 0, params ).f;
}
