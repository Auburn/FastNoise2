#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// SSA representation of a traced node tree
// Each op mirrors the semantics of the FS:: function it was traced from, so the program computes exactly what the SIMD code does
// Bitwise ops on floats are expressed as int ops between bitcasts, so backends only need integer bitwise ops

namespace FastNoise
{
    namespace CodeGen
    {
        namespace IR
        {
            enum class Type : uint8_t
            {
                F32,
                I32,
                Bool,
            };

            enum class Op : uint8_t
            {
                Param, // imm = parameter index
                Const, // imm = value bits
                Undef, // Uninitialised register, folded away by Select

                // Float
                FAdd,
                FSub,
                FMul,
                FDiv,
                FMin, // b < a ? b : a, matching std::min (and GLSL)
                FMax, // a < b ? b : a, matching std::max
                FMulAdd, // a * b + c
                FNeg,
                FAbs,
                FFloor,
                FCeil,
                FTrunc,
                FRound, // Round half to even
                FSqrt,
                FInvSqrt,
                FRcp,
                FSin, // FastSIMD polynomial approximations, reproduced exactly by EvaluateOp
                FCos,
                FExp,
                FLog, // a > 0 ? log( a ) : 0
                FPow, // a > 0 ? exp( b * log( a ) ) : 0
                FMod, // a - trunc( a / b ) * b

                // Int, wrapping
                IAdd,
                ISub,
                IMul,
                IMin,
                IMax,
                IAnd,
                IOr,
                IXor,
                IAndNot, // a & ~b
                INeg,
                INot,
                IAbs,
                IShl, // imm = shift amount
                IShrA, // Arithmetic, imm = shift amount
                IShrL, // Logical, imm = shift amount

                // Compare, result is Bool
                FEq,
                FNe,
                FLt,
                FLe,
                FGt,
                FGe,
                IEq,
                INe,
                ILt,
                ILe,
                IGt,
                IGe,

                // Bool
                BAnd,
                BOr,
                BXor,
                BAndNot, // a & ~b
                BNot,

                Select, // a ? b : c

                // Conversion
                F2I, // Round half to even, out of range gives INT32_MIN
                I2F,
                BitcastF2I,
                BitcastI2F,
                B2I, // true = -1, false = 0

                Call, // imm = function index, args = function parameters

                Count
            };

            using ValueId = int32_t;
            static constexpr ValueId kNoValue = -1;
            static constexpr int kMaxArgs = 5;

            struct Instr
            {
                Op op;
                Type type;
                uint8_t argCount = 0;
                ValueId args[kMaxArgs] = { kNoValue, kNoValue, kNoValue, kNoValue, kNoValue };
                int32_t imm = 0;

                bool operator==( const Instr& rhs ) const;
            };

            struct Parameter
            {
                Type type;
                std::string name;
            };

            // A node traced for a dimension count, or a helper function
            struct Function
            {
                std::string name; // Node or helper name
                std::string native; // Helpers only: C++ function with feature set specific implementations, called by C++ output in place of the traced body
                std::vector<Parameter> params; // Node functions take the seed followed by positions
                std::vector<Instr> code; // Operands always precede their users
                ValueId result = kNoValue;
            };

            struct Program
            {
                std::vector<Function> functions; // functions[0] is the entry point
            };

            union Scalar
            {
                float f;
                int32_t i;
            };

            const char* GetOpName( Op op );

            bool IsCommutative( Op op );

            // Scalar evaluation matching FastSIMD's non relaxed semantics exactly, used for constant folding and the reference interpreter
            Scalar EvaluateOp( Op op, Type argType, const Scalar* args, int32_t imm );

            // Builds SSA code with hash consing and constant folding, ops are only appended to the current function
            class Builder
            {
            public:
                Builder( Function& function, bool fastMath );
                ~Builder();

                Builder( const Builder& ) = delete;
                Builder& operator=( const Builder& ) = delete;

                static Builder& Current();

                ValueId Param( int index, Type type );
                ValueId ConstF( float value );
                ValueId ConstI( int32_t value );
                ValueId ConstB( bool value );
                ValueId Undef( Type type );

                ValueId Emit( Op op, ValueId a );
                ValueId Emit( Op op, ValueId a, ValueId b );
                ValueId Emit( Op op, ValueId a, ValueId b, ValueId c );
                ValueId EmitShift( Op op, ValueId a, int32_t amount );
                ValueId EmitCall( int function, const ValueId* args, int argCount );

                const Instr& Get( ValueId v ) const { return mFunction.code[v]; }
                Type GetType( ValueId v ) const { return mFunction.code[v].type; }

            private:
                struct InstrHash
                {
                    size_t operator()( const Instr& instr ) const;
                };

                ValueId Add( Instr instr );
                ValueId Simplify( Instr& instr );
                bool IsConst( ValueId v ) const;
                bool IsConstF( ValueId v, float value ) const;
                bool IsConstI( ValueId v, int32_t value ) const;

                Function& mFunction;
                std::unordered_map<Instr, ValueId, InstrHash> mCse;
                bool mFastMath;
                Builder* mPrevious;
            };

            // Instructions contributing to the function result
            std::vector<bool> FindLive( const Function& function );

            // Removes unreachable functions and dead instructions, renumbering what remains
            void Compact( Program& program );

            // Count of live instructions, excluding params and constants
            size_t CountOps( const Function& function );

            // Reference interpreter for the entry point
            float Evaluate( const Program& program, int32_t seed, const float* positions );
        }
    }
}
