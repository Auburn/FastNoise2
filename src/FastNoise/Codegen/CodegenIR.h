#pragma once
// Intermediate representation produced by tracing node code with the symbolic
// register types in CodegenRegister.h, consumed by the emitters in CodegenEmit.cpp.
//
// One Function is traced per (node instance, dimension count). Instructions are
// SSA style, stored in evaluation order (so every operand index is smaller than
// the index of the instruction using it) and hash-consed per function.

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace FastNoise
{
    class Generator;
}

namespace FastNoise::Codegen
{
    enum class Type : uint8_t
    {
        F32,
        I32,
        Bool,
    };

    enum class Op : uint8_t
    {
        // Leaves
        Param,      // imm.i = parameter index (0 = seed, 1.. = position components)
        ConstF,     // imm.f
        ConstI,     // imm.i
        Undef,      // default constructed register that was read before being written

        // Float arithmetic
        FAdd, FSub, FMul, FDiv, FNeg,
        FAnd, FOr, FXor, FNot,              // bitwise on the float bit pattern
        FMulAdd, FMulSub, FNMulAdd, FNMulSub, // a*b+c, a*b-c, c-a*b, -c-a*b
        FAbs, FFloor, FCeil, FTrunc, FRoundEven,
        FMin, FMax,
        FSqrt, FInvSqrt, FExp, FLog, FPow, FSin, FCos,

        // Integer arithmetic (32 bit, wrapping)
        IAdd, ISub, IMul, INeg,
        IAnd, IOr, IXor, INot,
        IShl, IShrArith, IShrLogical,       // shift amount in imm.i

        // Comparisons -> Bool
        FEq, FNe, FLt, FLe, FGt, FGe,
        IEq, INe, ILt, ILe, IGt, IGe,

        // Bool logic
        BAnd, BOr, BXor, BNot,

        // Select( a = condition, b = ifTrue, c = ifFalse ), result type = type of b
        Select,

        // Conversions
        F2I,        // round to nearest even, then truncate (matches FS::Convert<int32_t>)
        I2F,
        BitcastF2I,
        BitcastI2F,
        B2I,        // true -> -1 (all bits set), false -> 0, matches FS::Cast<int32_t>( mask )

        // Call another traced function: imm.i = function index, args = seed + positions
        Call,
    };

    static constexpr int kMaxArgs = 5; // seed + up to 4 position components

    struct Instr
    {
        Op op;
        Type type;
        uint8_t argCount = 0;
        int32_t args[kMaxArgs] = { -1, -1, -1, -1, -1 };
        union
        {
            float f;
            int32_t i;
        } imm = { 0.0f };

        bool operator==( const Instr& rhs ) const
        {
            return op == rhs.op && type == rhs.type && argCount == rhs.argCount &&
                   std::memcmp( args, rhs.args, sizeof( args ) ) == 0 && imm.i == rhs.imm.i;
        }
    };

    struct InstrHash
    {
        size_t operator()( const Instr& i ) const noexcept
        {
            uint64_t h = 1469598103934665603ull;
            auto mix = [&]( uint64_t v ) { h ^= v; h *= 1099511628211ull; };
            mix( (uint8_t)i.op ); mix( (uint8_t)i.type ); mix( i.argCount );
            for( int a : i.args ) mix( (uint32_t)a );
            mix( (uint32_t)i.imm.i );
            return (size_t)h;
        }
    };

    struct Function
    {
        const Generator* node = nullptr; // node instance this function was traced from
        std::string name;                // e.g. fn_Simplex_0_3D
        int dimensions = 0;              // number of position components
        std::vector<Instr> instrs;
        int32_t result = -1;             // instruction index of the return value
    };

    struct Module
    {
        std::vector<Function> functions;
        int32_t entry = -1; // index of the root node's function
    };

    // Appends instructions to a Function with hash-consing (common sub-expression reuse).
    class Builder
    {
    public:
        explicit Builder( Function& fn ) : mFn( fn ) {}

        // Appends an instruction, reusing an identical earlier one (hash-consing).
        // Out of line: it is called from thousands of sites in the instantiated node code.
        int32_t Emit( Instr instr );

        int32_t Leaf( Op op, Type type, int32_t imm )
        {
            Instr i; i.op = op; i.type = type; i.imm.i = imm;
            return Emit( i );
        }

        int32_t ConstF( float f )
        {
            Instr i; i.op = Op::ConstF; i.type = Type::F32; i.imm.f = f;
            return Emit( i );
        }

        int32_t ConstI( int32_t v ) { return Leaf( Op::ConstI, Type::I32, v ); }

        int32_t Unary( Op op, Type type, int32_t a, int32_t imm = 0 )
        {
            Instr i; i.op = op; i.type = type; i.argCount = 1; i.args[0] = a; i.imm.i = imm;
            return Emit( i );
        }

        int32_t Binary( Op op, Type type, int32_t a, int32_t b )
        {
            Instr i; i.op = op; i.type = type; i.argCount = 2; i.args[0] = a; i.args[1] = b;
            return Emit( i );
        }

        int32_t Ternary( Op op, Type type, int32_t a, int32_t b, int32_t c )
        {
            Instr i; i.op = op; i.type = type; i.argCount = 3; i.args[0] = a; i.args[1] = b; i.args[2] = c;
            return Emit( i );
        }

        int32_t Call( int32_t functionIdx, const int32_t* args, int argCount )
        {
            Instr i; i.op = Op::Call; i.type = Type::F32; i.argCount = (uint8_t)argCount; i.imm.i = functionIdx;
            for( int a = 0; a < argCount; a++ ) i.args[a] = args[a];
            return Emit( i );
        }

        Function& GetFunction() { return mFn; }

    private:
        int32_t Fold( const Instr& instr );

        Function& mFn;
        std::unordered_map<Instr, int32_t, InstrHash> mCse;
    };

    // Emitters (CodegenEmit.cpp)
    enum class Language;
    struct Options;
    std::string EmitModule( const Module& module, const Options& options );
}
