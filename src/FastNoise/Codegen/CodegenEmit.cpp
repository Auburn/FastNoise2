// Emits the traced IR as GLSL, HLSL, WGSL or scalar C++ source.

#include <FastNoise/Codegen.h>

#include "CodegenIR.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace FastNoise::Codegen
{
    namespace
    {
        const char* kParamNames[] = { "seed", "x", "y", "z", "w" };

        struct Emitter
        {
            const Module& module;
            const Options& options;
            Language lang;
            std::string out;

            // Per function state
            const Function* fn = nullptr;
            std::vector<int> uses;
            std::vector<bool> live;
            std::vector<bool> inlined;

            Emitter( const Module& m, const Options& o ) : module( m ), options( o ), lang( o.language ) {}

            bool Is( Language l ) const { return lang == l; }

            // ----- Language spellings -----

            const char* TypeName( Type t ) const
            {
                switch( t )
                {
                case Type::F32: return Is( Language::WGSL ) ? "f32" : "float";
                case Type::I32: return Is( Language::WGSL ) ? "i32" : Is( Language::CppScalar ) ? "int32_t" : "int";
                default: return "bool";
                }
            }

            std::string FloatBitsLit( uint32_t bits ) const
            {
                char buf[64];
                std::snprintf( buf, sizeof( buf ), "0x%08xu", bits );
                return FloatFromBits( buf );
            }

            std::string FloatFromBits( const std::string& intExpr ) const
            {
                switch( lang )
                {
                case Language::GLSL: return "intBitsToFloat(int(" + intExpr + "))";
                case Language::HLSL: return "asfloat(" + intExpr + ")";
                case Language::WGSL: return "bitcast<f32>(" + intExpr + ")";
                default: return "fn_bitsToFloat(" + intExpr + ")";
                }
            }

            std::string BitsFromFloat( const std::string& floatExpr ) const
            {
                switch( lang )
                {
                case Language::GLSL: return "floatBitsToInt(" + floatExpr + ")";
                case Language::HLSL: return "asint(" + floatExpr + ")";
                case Language::WGSL: return "bitcast<i32>(" + floatExpr + ")";
                default: return "fn_floatToBits(" + floatExpr + ")";
                }
            }

            std::string FloatLit( float f ) const
            {
                if( std::isinf( f ) )
                {
                    if( Is( Language::CppScalar ) )
                    {
                        return f < 0 ? "(-std::numeric_limits<float>::infinity())" : "std::numeric_limits<float>::infinity()";
                    }
                    return FloatBitsLit( f < 0 ? 0xff800000u : 0x7f800000u );
                }
                if( std::isnan( f ) )
                {
                    return Is( Language::CppScalar ) ? "std::numeric_limits<float>::quiet_NaN()" : FloatBitsLit( 0x7fc00000u );
                }

                char buf[64];
                std::snprintf( buf, sizeof( buf ), "%.9g", f );
                std::string s = buf;

                if( s.find_first_of( ".eE" ) == std::string::npos )
                {
                    s += ".0";
                }
                else if( s.find( '.' ) == std::string::npos && Is( Language::WGSL ) )
                {
                    // WGSL requires a decimal point or suffix when using an exponent with no fraction (e.g. 1e-05)
                    s += "f";
                }

                if( Is( Language::CppScalar ) )
                {
                    s += "f";
                }
                if( f < 0 )
                {
                    s = "(" + s + ")";
                }
                return s;
            }

            std::string IntLit( int32_t v ) const
            {
                if( v == std::numeric_limits<int32_t>::min() )
                {
                    switch( lang )
                    {
                    case Language::GLSL: return "int(0x80000000u)";
                    case Language::HLSL: return "int(0x80000000u)";
                    case Language::WGSL: return "bitcast<i32>(0x80000000u)";
                    default: return "std::numeric_limits<int32_t>::min()";
                    }
                }
                std::string s = std::to_string( v );
                return v < 0 ? "(" + s + ")" : s;
            }

            std::string ShiftAmount( int n ) const
            {
                return Is( Language::WGSL ) ? std::to_string( n ) + "u" : std::to_string( n );
            }

            std::string ToUint( const std::string& a ) const
            {
                switch( lang )
                {
                case Language::WGSL: return "bitcast<u32>(" + a + ")";
                case Language::CppScalar: return "uint32_t(" + a + ")";
                default: return "uint(" + a + ")";
                }
            }

            std::string FromUint( const std::string& a ) const
            {
                switch( lang )
                {
                case Language::WGSL: return "bitcast<i32>(" + a + ")";
                case Language::CppScalar: return "int32_t(" + a + ")";
                default: return "int(" + a + ")";
                }
            }

            std::string Select( const std::string& c, const std::string& a, const std::string& b ) const
            {
                if( Is( Language::WGSL ) )
                {
                    return "select(" + b + ", " + a + ", " + c + ")";
                }
                return "(" + c + " ? " + a + " : " + b + ")";
            }

            std::string Fn( const char* glsl, const char* hlsl, const char* wgsl, const char* cpp ) const
            {
                switch( lang )
                {
                case Language::GLSL: return glsl;
                case Language::HLSL: return hlsl;
                case Language::WGSL: return wgsl;
                default: return cpp;
                }
            }

            std::string RoundEven( const std::string& a ) const
            {
                return Fn( "roundEven", "fn_roundEven", "round", "std::nearbyint" ) + "(" + a + ")";
            }

            // ----- Expression generation -----

            std::string Operand( int32_t idx )
            {
                const Instr& in = fn->instrs[idx];

                if( inlined[idx] )
                {
                    std::string e = Expr( idx );
                    bool leaf = in.op == Op::Param || in.op == Op::ConstF || in.op == Op::ConstI || in.op == Op::Undef || in.op == Op::Call;
                    return leaf ? e : "(" + e + ")";
                }
                return "t" + std::to_string( idx );
            }

            std::string Expr( int32_t idx )
            {
                const Instr& in = fn->instrs[idx];
                auto A = [&] { return Operand( in.args[0] ); };
                auto B = [&] { return Operand( in.args[1] ); };
                auto C = [&] { return Operand( in.args[2] ); };

                switch( in.op )
                {
                case Op::Param: return kParamNames[in.imm.i];
                case Op::ConstF: return FloatLit( in.imm.f );
                case Op::ConstI: return IntLit( in.imm.i );
                case Op::Undef: return in.type == Type::F32 ? "0.0" : in.type == Type::I32 ? "0" : "false";

                case Op::FAdd: return A() + " + " + B();
                case Op::FSub: return A() + " - " + B();
                case Op::FMul: return A() + " * " + B();
                case Op::FDiv: return A() + " / " + B();
                case Op::FNeg: return "-" + A();
                case Op::FAnd: return FloatFromBits( BitsFromFloat( A() ) + " & " + BitsFromFloat( B() ) );
                case Op::FOr: return FloatFromBits( BitsFromFloat( A() ) + " | " + BitsFromFloat( B() ) );
                case Op::FXor: return FloatFromBits( BitsFromFloat( A() ) + " ^ " + BitsFromFloat( B() ) );
                case Op::FNot: return FloatFromBits( "~" + BitsFromFloat( A() ) );

                case Op::FMulAdd:
                    if( options.emitFma ) return Fn( "fma", "mad", "fma", "std::fma" ) + "(" + A() + ", " + B() + ", " + C() + ")";
                    return A() + " * " + B() + " + " + C();
                case Op::FMulSub:
                    if( options.emitFma ) return Fn( "fma", "mad", "fma", "std::fma" ) + "(" + A() + ", " + B() + ", -" + C() + ")";
                    return A() + " * " + B() + " - " + C();
                case Op::FNMulAdd:
                    if( options.emitFma ) return Fn( "fma", "mad", "fma", "std::fma" ) + "(-" + A() + ", " + B() + ", " + C() + ")";
                    return C() + " - " + A() + " * " + B();
                case Op::FNMulSub:
                    if( options.emitFma ) return Fn( "fma", "mad", "fma", "std::fma" ) + "(-" + A() + ", " + B() + ", -" + C() + ")";
                    return "-" + C() + " - " + A() + " * " + B();

                case Op::FAbs: return Fn( "abs", "abs", "abs", "std::fabs" ) + "(" + A() + ")";
                case Op::FFloor: return Fn( "floor", "floor", "floor", "std::floor" ) + "(" + A() + ")";
                case Op::FCeil: return Fn( "ceil", "ceil", "ceil", "std::ceil" ) + "(" + A() + ")";
                case Op::FTrunc: return Fn( "trunc", "trunc", "trunc", "std::trunc" ) + "(" + A() + ")";
                case Op::FRoundEven: return RoundEven( A() );
                case Op::FMin: return Fn( "min", "min", "min", "std::fmin" ) + "(" + A() + ", " + B() + ")";
                case Op::FMax: return Fn( "max", "max", "max", "std::fmax" ) + "(" + A() + ", " + B() + ")";
                case Op::FSqrt: return Fn( "sqrt", "sqrt", "sqrt", "std::sqrt" ) + "(" + A() + ")";
                case Op::FInvSqrt:
                    if( Is( Language::CppScalar ) ) return "(1.0f / std::sqrt(" + A() + "))";
                    return Fn( "inversesqrt", "rsqrt", "inverseSqrt", "" ) + "(" + A() + ")";
                case Op::FExp: return Fn( "exp", "exp", "exp", "std::exp" ) + "(" + A() + ")";
                case Op::FLog: return Fn( "log", "log", "log", "std::log" ) + "(" + A() + ")";
                case Op::FSin: return Fn( "sin", "sin", "sin", "std::sin" ) + "(" + A() + ")";
                case Op::FCos: return Fn( "cos", "cos", "cos", "std::cos" ) + "(" + A() + ")";
                case Op::FPow:
                    // FastSIMD's Pow returns 0 for value <= 0
                    return Select( A() + " > 0.0", Fn( "pow", "pow", "pow", "std::pow" ) + "(" + A() + ", " + B() + ")", "0.0" );

                // Integer arithmetic must wrap; signed overflow is undefined in C++ and
                // underspecified in GLSL, so route through unsigned where it matters
                case Op::IAdd: return Is( Language::CppScalar ) ? FromUint( ToUint( A() ) + " + " + ToUint( B() ) ) : A() + " + " + B();
                case Op::ISub: return Is( Language::CppScalar ) ? FromUint( ToUint( A() ) + " - " + ToUint( B() ) ) : A() + " - " + B();
                case Op::IMul: return FromUint( ToUint( A() ) + " * " + ToUint( B() ) );
                case Op::INeg: return Is( Language::CppScalar ) ? FromUint( "0u - " + ToUint( A() ) ) : "-" + A();
                case Op::IAnd: return A() + " & " + B();
                case Op::IOr: return A() + " | " + B();
                case Op::IXor: return A() + " ^ " + B();
                case Op::INot: return "~" + A();
                case Op::IShl: return Is( Language::CppScalar ) ? FromUint( ToUint( A() ) + " << " + ShiftAmount( in.imm.i ) ) : A() + " << " + ShiftAmount( in.imm.i );
                case Op::IShrArith: return A() + " >> " + ShiftAmount( in.imm.i );
                case Op::IShrLogical: return FromUint( ToUint( A() ) + " >> " + ShiftAmount( in.imm.i ) );

                case Op::FEq: case Op::IEq: return A() + " == " + B();
                case Op::FNe: case Op::INe: return A() + " != " + B();
                case Op::FLt: case Op::ILt: return A() + " < " + B();
                case Op::FLe: case Op::ILe: return A() + " <= " + B();
                case Op::FGt: case Op::IGt: return A() + " > " + B();
                case Op::FGe: case Op::IGe: return A() + " >= " + B();

                case Op::BAnd: return A() + " && " + B();
                case Op::BOr: return A() + " || " + B();
                case Op::BXor: return A() + " != " + B();
                case Op::BNot: return "!" + A();

                case Op::Select: return Select( A(), B(), C() );

                case Op::F2I:
                    if( Is( Language::CppScalar ) ) return "fn_floatToInt(" + A() + ")";
                    return ( Is( Language::WGSL ) ? "i32(" : "int(" ) + RoundEven( A() ) + ")";
                case Op::I2F: return ( Is( Language::WGSL ) ? "f32(" : "float(" ) + A() + ")";
                case Op::BitcastF2I: return BitsFromFloat( A() );
                case Op::BitcastI2F: return FloatFromBits( A() );
                case Op::B2I: return Select( A(), "-1", "0" );

                case Op::Call:
                {
                    std::string s = module.functions[in.imm.i].name + "(";
                    for( int a = 0; a < in.argCount; a++ )
                    {
                        if( a ) s += ", ";
                        s += Operand( in.args[a] );
                    }
                    return s + ")";
                }
                }
                return "/* unknown op */";
            }

            // ----- Function emission -----

            std::string Signature( const Function& f, bool prototype )
            {
                std::string s;
                if( Is( Language::WGSL ) )
                {
                    s = "fn " + f.name + "(seed: i32";
                    for( int i = 0; i < f.dimensions; i++ )
                    {
                        s += std::string( ", " ) + kParamNames[i + 1] + ": f32";
                    }
                    s += ") -> f32";
                }
                else
                {
                    s = std::string( Is( Language::CppScalar ) ? "static inline float " : "float " ) + f.name + "(" + TypeName( Type::I32 ) + " seed";
                    for( int i = 0; i < f.dimensions; i++ )
                    {
                        s += std::string( ", float " ) + kParamNames[i + 1];
                    }
                    s += ")";
                }
                return prototype ? s + ";\n" : s;
            }

            void Analyse( const Function& f )
            {
                fn = &f;
                size_t n = f.instrs.size();
                uses.assign( n, 0 );
                live.assign( n, false );
                inlined.assign( n, false );

                // Reachability from the result (instructions are in dependency order, so a
                // reverse sweep marks everything live before visiting its operands)
                if( f.result >= 0 )
                {
                    live[f.result] = true;
                }
                for( size_t i = n; i-- > 0; )
                {
                    if( !live[i ]) continue;
                    const Instr& in = f.instrs[i];
                    for( int a = 0; a < in.argCount; a++ )
                    {
                        live[in.args[a]] = true;
                        uses[in.args[a]]++;
                    }
                }

                // Single use instructions are inlined into their user, up to a nesting depth
                // that keeps lines readable and below compiler bracket limits (cellular
                // distance sorting otherwise produces a chain of 81 nested min/max calls)
                constexpr int kMaxInlineDepth = 6;
                std::vector<int> depth( n, 0 );

                for( size_t i = 0; i < n; i++ )
                {
                    const Instr& in = f.instrs[i];
                    bool leaf = in.op == Op::Param || in.op == Op::ConstF || in.op == Op::ConstI || in.op == Op::Undef;
                    // Keep calls and selects as named temporaries for readability
                    bool keep = in.op == Op::Call || in.op == Op::Select;

                    int d = 0;
                    for( int a = 0; a < in.argCount; a++ )
                    {
                        if( inlined[in.args[a]] ) d = std::max( d, depth[in.args[a]] );
                    }
                    depth[i] = leaf ? 0 : d + 1;

                    inlined[i] = live[i] && ( leaf || ( uses[i] == 1 && !keep && depth[i] <= kMaxInlineDepth && (int32_t)i != f.result ) );
                }
            }

            void EmitFunction( const Function& f )
            {
                Analyse( f );

                out += Signature( f, false ) + "\n{\n";

                for( size_t i = 0; i < f.instrs.size(); i++ )
                {
                    if( !live[i] || inlined[i] || (int32_t)i == f.result )
                    {
                        continue;
                    }
                    const Instr& in = f.instrs[i];
                    std::string name = "t" + std::to_string( i );

                    if( Is( Language::WGSL ) )
                    {
                        out += "    let " + name + ": " + TypeName( in.type ) + " = " + Expr( (int32_t)i ) + ";\n";
                    }
                    else
                    {
                        out += "    " + std::string( TypeName( in.type ) ) + " " + name + " = " + Expr( (int32_t)i ) + ";\n";
                    }
                }

                out += "    return " + ( f.result >= 0 ? Expr( f.result ) : std::string( "0.0" ) ) + ";\n}\n\n";
            }

            void EmitPreamble()
            {
                out += "// Generated by FastNoise2 Codegen\n\n";

                switch( lang )
                {
                case Language::HLSL:
                    out += "float fn_roundEven(float x)\n{\n    float r = round(x);\n    if (abs(x - trunc(x)) == 0.5) { r = 2.0 * round(x * 0.5); }\n    return r;\n}\n\n";
                    break;
                case Language::CppScalar:
                    out += "#include <cmath>\n#include <cstdint>\n#include <cstring>\n#include <limits>\n\n"
                           "static inline int32_t fn_floatToBits(float f) { int32_t i; std::memcpy(&i, &f, 4); return i; }\n"
                           "static inline float fn_bitsToFloat(int32_t i) { float f; std::memcpy(&f, &i, 4); return f; }\n"
                           "static inline int32_t fn_floatToInt(float f) { float r = std::nearbyint(f); return (r >= 2147483648.0f || r < -2147483648.0f || r != r) ? std::numeric_limits<int32_t>::min() : (int32_t)r; }\n\n";
                    break;
                default:
                    break;
                }
            }

            void EmitEntry()
            {
                const Function& root = module.functions[module.entry];
                std::string args = "seed";
                for( int i = 0; i < root.dimensions; i++ )
                {
                    args += std::string( ", " ) + kParamNames[i + 1];
                }

                Function entry = root;
                entry.name = options.entryPoint;
                out += Signature( entry, false ) + "\n{\n    return " + root.name + "(" + args + ");\n}\n";

                // Vector position overload for shader languages
                const char* vecType = nullptr;
                switch( lang )
                {
                case Language::GLSL: vecType = root.dimensions == 2 ? "vec2" : root.dimensions == 3 ? "vec3" : "vec4"; break;
                case Language::HLSL: vecType = root.dimensions == 2 ? "float2" : root.dimensions == 3 ? "float3" : "float4"; break;
                case Language::WGSL: vecType = root.dimensions == 2 ? "vec2<f32>" : root.dimensions == 3 ? "vec3<f32>" : "vec4<f32>"; break;
                default: break;
                }
                if( vecType )
                {
                    std::string swizzle = "pos.x, pos.y";
                    if( root.dimensions >= 3 ) swizzle += ", pos.z";
                    if( root.dimensions >= 4 ) swizzle += ", pos.w";

                    if( Is( Language::WGSL ) )
                    {
                        out += "\nfn " + std::string( options.entryPoint ) + "_vec(seed: i32, pos: " + vecType + ") -> f32\n{\n    return " + root.name + "(seed, " + swizzle + ");\n}\n";
                    }
                    else
                    {
                        out += "\nfloat " + std::string( options.entryPoint ) + "(int seed, " + vecType + " pos)\n{\n    return " + root.name + "(seed, " + swizzle + ");\n}\n";
                    }
                }
            }

            std::string Run()
            {
                EmitPreamble();

                if( !Is( Language::WGSL ) )
                {
                    for( const Function& f : module.functions )
                    {
                        out += Signature( f, true );
                    }
                    out += "\n";
                }

                // Callees were requested after their callers, so emitting in reverse order
                // defines every function before it is used (needed by GLSL without prototypes)
                for( size_t i = module.functions.size(); i-- > 0; )
                {
                    EmitFunction( module.functions[i] );
                }

                EmitEntry();
                return out;
            }
        };
    }

    std::string EmitModule( const Module& module, const Options& options )
    {
        if( module.entry < 0 )
        {
            return "";
        }
        Emitter emitter( module, options );
        return emitter.Run();
    }
}
