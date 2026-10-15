#include "CodegenIR.h"

#include <cstring>

namespace FastNoise::Codegen
{
    int32_t Builder::Emit( Instr instr )
    {
        int32_t folded = Fold( instr );
        if( folded >= 0 )
        {
            return folded;
        }

        auto find = mCse.find( instr );
        if( find != mCse.end() )
        {
            return find->second;
        }
        int32_t idx = (int32_t)mFn.instrs.size();
        mFn.instrs.push_back( instr );
        mCse.emplace( instr, idx );
        return idx;
    }

    // Constant folding and identities. Node code broadcasts member values into registers, so
    // without this the output is littered with `x * 1.0` and `seed + 0`. Everything here is
    // exact for finite values; `x * 0` and the `x + 0` sign of zero follow the relaxed CPU build.
    int32_t Builder::Fold( const Instr& in )
    {
        auto isConstF = [&]( int32_t a, float& v ) { if( a < 0 || mFn.instrs[a].op != Op::ConstF ) return false; v = mFn.instrs[a].imm.f; return true; };
        auto isConstI = [&]( int32_t a, int32_t& v ) { if( a < 0 || mFn.instrs[a].op != Op::ConstI ) return false; v = mFn.instrs[a].imm.i; return true; };
        auto bitsEq = [&]( float a, float b ) { return std::memcmp( &a, &b, sizeof( float ) ) == 0; };
        auto rewrite = [&]( Op op, int32_t a, int32_t b ) { Instr r = in; r.op = op; r.argCount = 2; r.args[0] = a; r.args[1] = b; r.args[2] = -1; return Emit( r ); };

        float fa, fb, fc;
        int32_t ia, ib;

        switch( in.op )
        {
        case Op::FAdd:
            if( isConstF( in.args[0], fa ) && isConstF( in.args[1], fb ) ) return ConstF( fa + fb );
            if( isConstF( in.args[1], fb ) && bitsEq( fb, 0.0f ) ) return in.args[0];
            if( isConstF( in.args[0], fa ) && bitsEq( fa, 0.0f ) ) return in.args[1];
            break;
        case Op::FSub:
            if( isConstF( in.args[0], fa ) && isConstF( in.args[1], fb ) ) return ConstF( fa - fb );
            if( isConstF( in.args[1], fb ) && bitsEq( fb, 0.0f ) ) return in.args[0];
            if( isConstF( in.args[1], fb ) && fb < 0.0f ) return rewrite( Op::FAdd, in.args[0], ConstF( -fb ) ); // a - (-c) prints as a + c
            break;
        case Op::FMul:
            if( isConstF( in.args[0], fa ) && isConstF( in.args[1], fb ) ) return ConstF( fa * fb );
            if( isConstF( in.args[1], fb ) && fb == 1.0f ) return in.args[0];
            if( isConstF( in.args[0], fa ) && fa == 1.0f ) return in.args[1];
            if( isConstF( in.args[1], fb ) && bitsEq( fb, 0.0f ) ) return ConstF( 0.0f );
            if( isConstF( in.args[0], fa ) && bitsEq( fa, 0.0f ) ) return ConstF( 0.0f );
            break;
        case Op::FDiv:
            if( isConstF( in.args[0], fa ) && isConstF( in.args[1], fb ) ) return ConstF( fa / fb );
            if( isConstF( in.args[1], fb ) && fb == 1.0f ) return in.args[0];
            break;
        case Op::FNeg:
            if( isConstF( in.args[0], fa ) ) return ConstF( -fa );
            break;
        case Op::FMulAdd:
            if( isConstF( in.args[2], fc ) && bitsEq( fc, 0.0f ) ) return rewrite( Op::FMul, in.args[0], in.args[1] );
            if( isConstF( in.args[0], fa ) && fa == 1.0f ) return rewrite( Op::FAdd, in.args[1], in.args[2] );
            if( isConstF( in.args[1], fb ) && fb == 1.0f ) return rewrite( Op::FAdd, in.args[0], in.args[2] );
            if( isConstF( in.args[0], fa ) && bitsEq( fa, 0.0f ) ) return in.args[2];
            if( isConstF( in.args[1], fb ) && bitsEq( fb, 0.0f ) ) return in.args[2];
            break;
        case Op::IAdd:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( (int32_t)( (uint32_t)ia + (uint32_t)ib ) );
            if( isConstI( in.args[1], ib ) && ib == 0 ) return in.args[0];
            if( isConstI( in.args[0], ia ) && ia == 0 ) return in.args[1];
            break;
        case Op::ISub:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( (int32_t)( (uint32_t)ia - (uint32_t)ib ) );
            if( isConstI( in.args[1], ib ) && ib == 0 ) return in.args[0];
            if( isConstI( in.args[1], ib ) && ib < 0 && ib != std::numeric_limits<int32_t>::min() ) return rewrite( Op::IAdd, in.args[0], ConstI( -ib ) );
            break;
        case Op::IMul:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( (int32_t)( (uint32_t)ia * (uint32_t)ib ) );
            if( isConstI( in.args[1], ib ) && ib == 1 ) return in.args[0];
            if( isConstI( in.args[0], ia ) && ia == 1 ) return in.args[1];
            break;
        case Op::IAnd:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( ia & ib );
            break;
        case Op::IOr:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( ia | ib );
            if( isConstI( in.args[1], ib ) && ib == 0 ) return in.args[0];
            break;
        case Op::IXor:
            if( isConstI( in.args[0], ia ) && isConstI( in.args[1], ib ) ) return ConstI( ia ^ ib );
            if( isConstI( in.args[1], ib ) && ib == 0 ) return in.args[0];
            break;
        case Op::INeg:
            if( isConstI( in.args[0], ia ) ) return ConstI( (int32_t)( 0u - (uint32_t)ia ) );
            break;
        case Op::INot:
            if( isConstI( in.args[0], ia ) ) return ConstI( ~ia );
            break;
        case Op::IShl:
            if( isConstI( in.args[0], ia ) ) return ConstI( (int32_t)( (uint32_t)ia << in.imm.i ) );
            break;
        case Op::IShrArith:
            if( isConstI( in.args[0], ia ) ) return ConstI( ia >> in.imm.i );
            break;
        case Op::IShrLogical:
            if( isConstI( in.args[0], ia ) ) return ConstI( (int32_t)( (uint32_t)ia >> in.imm.i ) );
            break;
        case Op::I2F:
            if( isConstI( in.args[0], ia ) ) return ConstF( (float)ia );
            break;
        case Op::BitcastI2F:
            if( isConstI( in.args[0], ia ) ) { std::memcpy( &fa, &ia, sizeof( float ) ); return ConstF( fa ); }
            break;
        case Op::BitcastF2I:
            if( isConstF( in.args[0], fa ) ) { std::memcpy( &ia, &fa, sizeof( float ) ); return ConstI( ia ); }
            break;
        case Op::Select:
            if( in.args[1] == in.args[2] ) return in.args[1];
            break;
        default:
            break;
        }
        return -1;
    }
}
