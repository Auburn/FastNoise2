#include <cfloat>
#include <climits>
#include <random>
#include <iostream>
#include <cmath>

#include "FastSIMD/FunctionList.h"
#include "../src/FastSIMD/Internal/Scalar.h"

#if FASTSIMD_x86
#include "../src/FastSIMD/Internal/SSE.h"
#include "../src/FastSIMD/Internal/AVX.h"
#include "../src/FastSIMD/Internal/AVX512.h"
#endif

#if FASTSIMD_ARM
#include "../src/FastSIMD/Internal/NEON.h"
#endif

#include <vector>
#include <functional>
#include <type_traits>

template<typename... T>
struct SIMDClassContainer
{
    using Top = void;

    template<typename L>
    using GetNext = void;
};

template<typename HEAD, typename... TAIL>
struct SIMDClassContainer<HEAD, TAIL...>
{
    using Top = HEAD;

    template<typename L>
    using GetNext = std::conditional_t<std::is_same_v<L, HEAD>, typename SIMDClassContainer<TAIL...>::Top, typename SIMDClassContainer<TAIL...>::template GetNext<L>>;
};

typedef SIMDClassContainer<
    FastSIMD::Scalar
#if FASTSIMD_x86
    ,
    FastSIMD::SSE2,
    FastSIMD::SSE41,
    FastSIMD::AVX2,
    FastSIMD::AVX512
#endif
#if FASTSIMD_ARM
    ,
    FastSIMD::NEON
#endif
>
SIMDClassList;

class SIMDUnitTest
{
public:

    static void RunAll();

    SIMDUnitTest( std::function<void( void* )> func )
    {
        tests.emplace_back( func );
    }

private:
    inline static std::vector<std::function<void( void* )> > tests;

};

const std::size_t TestCount = 1073741824 / 16;
const std::size_t NonVecMask = ~15;

int  * rndInts0;
int  * rndInts1;
float* rndFloats0;
float* rndFloats1;

float GenNormalFloat( std::mt19937& gen )
{
    union
    {
        float f;
        int32_t i;
    } u;

    do
    {
        u.i = gen();

    } while ( !std::isnormal( u.f ) );

    return u.f;
}

void SIMDUnitTest::RunAll()
{
    rndInts0 = new int[TestCount];
    rndInts1 = new int[TestCount];
    rndFloats0 = new float[TestCount];
    rndFloats1 = new float[TestCount];

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()

    for ( std::size_t i = 0; i < TestCount; i++ )
    {
        rndInts0[i] = gen();
        rndInts1[i] = gen();
        rndFloats0[i] = GenNormalFloat( gen );
        rndFloats1[i] = GenNormalFloat( gen );
    }

    for ( const auto& test : tests )
    {
        test( nullptr );
    }

    delete[] rndInts0;
    delete[] rndInts1;
    delete[] rndFloats0;
    delete[] rndFloats1;
}

#define SIMD_FUNCTION_TEST( NAME, RETURN_TYPE, FUNC ) SIMD_FUNCTION_TEST_BASE( NAME, RETURN_TYPE, SIMDClassList::Top, FUNC )

#define SIMD_FUNCTION_TEST_BASE( NAME, RETURN_TYPE, LEVEL, FUNC )                                          \
template<typename T, typename FS>                                                                          \
std::enable_if_t<std::is_same<void, FS>::value> TestFunction_##NAME( void* baseData = nullptr )      \
{                                                                                                          \
    std::cout << "\n";                                                                                     \
    delete[] (T*)baseData;                                                                                     \
}                                                                                                          \
                                                                                                           \
template<typename T, typename FS>                                                                          \
std::enable_if_t<!std::is_same<void, FS>::value> TestFunction_##NAME( void* baseData = nullptr )     \
{                                                                                                          \
    bool isBase = baseData == nullptr;                                                                     \
                                                                                                           \
    if ( isBase )                                                                                          \
    {                                                                                                      \
        std::cout << #NAME " - Base: " << FS::SIMD_Level;                                                  \
        baseData = new T[TestCount];                                                                       \
    }                                                                                                      \
    else { std::cout << " Testing: " << FS::SIMD_Level; }                                                  \
                                                                                                           \
    if ( FS::SIMD_Level > FastSIMD::CPUMaxSIMDLevel() )                                                    \
    {                                                                                                      \
        std::cout << " CPU N//A: " << FS::SIMD_Level;                                                      \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
        T result[FS_Size_32()];                                                    \
        int failCount = 0;                                                                                    \
                                                                                                           \
        for ( std::size_t i = 0; i < TestCount; i += FS_Size_32() )                \
        {                                                                                                  \
            FUNC;                                                                                          \
                                                                                                           \
            for ( std::size_t ir = 0; ir < FS_Size_32(); ir++ )                    \
            {                                                                                              \
                if ( isBase )                                                                              \
                {                                                                                          \
                    ((T*)baseData)[i + ir] = result[ir];                                                   \
                }                                                                                          \
                else if ( result[ir] != ((T*)baseData)[i + ir] &&                                          \
                    (result[ir] == result[ir] ||                                                           \
                    ((T*)baseData)[i + ir] == ((T*)baseData)[i + ir]) )                                    \
                {                                                                                           \
                    failCount++;                                                                                           \
                    std::cout << "\n" << FS::SIMD_Level << " Failed: expected: " << ((T*)baseData)[i + ir];                         \
                    std::cout << " actual: " << result[ir] << " index: " << i+ir;                          \
                    if(std::is_integral_v<T>) std::cout << " ints: " << rndInts0[i + ir] << " : " << rndInts1[i + ir];               \
                    else std::cout << " floats: " << rndFloats0[i + ir] << " : " << rndFloats1[i + ir] << "\n"; \
                }                                                                                          \
            }                                                                                              \
            if( failCount >= 32 ) break;                                                                    \
        }                                                                                                  \
    }                                                                                                      \
                                                                                                           \
    TestFunction_##NAME<T, SIMDClassList::GetNext<FS>>( baseData );                \
}                                                                                                          \
SIMDUnitTest test_##NAME( TestFunction_##NAME<RETURN_TYPE, LEVEL> );

SIMD_FUNCTION_TEST( LoadStore_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) ) )

SIMD_FUNCTION_TEST( LoadStore_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) ) )


SIMD_FUNCTION_TEST( Casti32_f32, float, FS_Store_f32( &result, FS_Casti32_f32( FS_Load_i32( &rndInts0[i] ) ) ) )

SIMD_FUNCTION_TEST( Castf32_i32, int32_t, FS_Store_i32( &result, FS_Castf32_i32( FS_Load_f32( &rndFloats0[i] ) ) ) )

SIMD_FUNCTION_TEST( Converti32_f32, float, FS_Store_f32( &result, FS_Converti32_f32( FS_Load_i32( &rndInts0[i] ) ) ) )

SIMD_FUNCTION_TEST( Convertf32_i32, int32_t, FS_Store_i32( &result, FS_Convertf32_i32( FS_Load_f32( &rndFloats0[i] ) ) ) )


SIMD_FUNCTION_TEST( Equal_f32, float, FS_Store_f32( &result, FS_Mask_f32( typename FS::float32v( 1 ), ( FS_Load_f32( &rndFloats0[i] ) == FS_Load_f32( &rndFloats1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( GreaterThan_f32, float, FS_Store_f32( &result, FS_Mask_f32( typename FS::float32v( 1 ), ( FS_Load_f32( &rndFloats0[i] ) > FS_Load_f32( &rndFloats1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( LessThan_f32, float, FS_Store_f32( &result, FS_Mask_f32( typename FS::float32v( 1 ), ( FS_Load_f32( &rndFloats0[i] ) < FS_Load_f32( &rndFloats1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( GreaterEqualThan_f32, float, FS_Store_f32( &result, FS_Mask_f32( typename FS::float32v( 1 ), ( FS_Load_f32( &rndFloats0[i] ) >= FS_Load_f32( &rndFloats1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( LessEqualThan_f32, float, FS_Store_f32( &result, FS_Mask_f32( typename FS::float32v( 1 ), ( FS_Load_f32( &rndFloats0[i] ) <= FS_Load_f32( &rndFloats1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( Equal_i32, int32_t, FS_Store_i32( &result, FS_Mask_i32( typename FS::int32v( 1 ), ( FS_Load_i32( &rndInts0[i] ) == FS_Load_i32( &rndInts1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( GreaterThan_i32, int32_t, FS_Store_i32( &result, FS_Mask_i32( typename FS::int32v( 1 ), ( FS_Load_i32( &rndInts0[i] ) > FS_Load_i32( &rndInts1[i] ) ) ) ) )

SIMD_FUNCTION_TEST( LessThan_i32, int32_t, FS_Store_i32( &result, FS_Mask_i32( typename FS::int32v( 1 ), ( FS_Load_i32( &rndInts0[i] ) < FS_Load_i32( &rndInts1[i] ) ) ) ) )


SIMD_FUNCTION_TEST( Select_f32, float, FS_Store_f32( &result, FS_Select_f32( ( FS_Load_f32( &rndFloats0[i] ) > FS_Load_f32( &rndFloats1[i] ) ), FS_Load_f32( &rndFloats0[i] ), FS_Load_f32( &rndFloats1[i] ) ) ) )

SIMD_FUNCTION_TEST( Select_i32, int32_t, FS_Store_i32( &result, FS_Select_i32( ( FS_Load_i32( &rndInts0[i] ) > FS_Load_i32( &rndInts1[i] ) ), FS_Load_i32( &rndInts0[i] ), FS_Load_i32( &rndInts1[i] ) ) ) )


SIMD_FUNCTION_TEST( Min_f32, float, FS_Store_f32( &result, FS_Min_f32( FS_Load_f32( &rndFloats0[i] ), FS_Load_f32( &rndFloats1[i] ) ) ) )

SIMD_FUNCTION_TEST( Max_f32, float, FS_Store_f32( &result, FS_Max_f32( FS_Load_f32( &rndFloats0[i] ), FS_Load_f32( &rndFloats1[i] ) ) ) )

SIMD_FUNCTION_TEST( Min_i32, int32_t, FS_Store_i32( &result, FS_Min_i32( FS_Load_i32( &rndInts0[i] ), FS_Load_i32( &rndInts1[i] ) ) ) )

SIMD_FUNCTION_TEST( Max_i32, int32_t, FS_Store_i32( &result, FS_Max_i32( FS_Load_i32( &rndInts0[i] ), FS_Load_i32( &rndInts1[i] ) ) ) )


SIMD_FUNCTION_TEST( BitwiseAndNot_f32, float, FS_Store_f32( &result, FS_BitwiseAndNot_f32( FS_Load_f32( &rndFloats0[i] ), FS_Load_f32( &rndFloats1[i] ) ) ) )

SIMD_FUNCTION_TEST( BitwiseAndNot_i32, int32_t, FS_Store_i32( &result, FS_BitwiseAndNot_i32( FS_Load_i32( &rndInts0[i] ), FS_Load_i32( &rndInts1[i] ) ) ) )


SIMD_FUNCTION_TEST( BitwiseShiftRightZX_f32, float, FS_Store_f32( &result, FS_BitwiseShiftRightZX_f32( FS_Load_f32( &rndFloats0[i] ), (rndInts1[i & NonVecMask] & 31) ) ) )

SIMD_FUNCTION_TEST( BitwiseShiftRightZX_i32, int32_t, FS_Store_i32( &result, FS_BitwiseShiftRightZX_i32( FS_Load_i32( &rndInts0[i] ), (rndInts1[i & NonVecMask] & 31) ) ) )


SIMD_FUNCTION_TEST( Abs_f32, float, FS_Store_f32( &result, FS_Abs_f32( FS_Load_f32( &rndFloats0[i] ) ) ) )

SIMD_FUNCTION_TEST( Abs_i32, int32_t, FS_Store_i32( &result, FS_Abs_i32( FS_Load_i32( &rndInts0[i] ) ) ) )

SIMD_FUNCTION_TEST( Sqrt_f32, float, FS_Store_f32( &result, FS_Sqrt_f32( FS_Load_f32( &rndFloats0[i] ) ) ) )

//SIMD_FUNCTION_TEST( InvSqrt_f32, float, FS_Store_f32( &result, FS_InvSqrt_f32( FS_Load_f32( &rndFloats0[i] ) ) ) )


const float MAX_ROUNDING = (float)INT_MAX / 2.0f;

SIMD_FUNCTION_TEST( Floor_f32, float, FS_Store_f32( &result, FS_Floor_f32( typename FS::float32v( MAX_ROUNDING / FLT_MAX ) * FS_Load_f32( &rndFloats0[i] ) ) ) )

SIMD_FUNCTION_TEST( Ceil_f32, float, FS_Store_f32( &result, FS_Ceil_f32( typename FS::float32v( MAX_ROUNDING / FLT_MAX ) * FS_Load_f32( &rndFloats0[i] ) ) ) )

//SIMD_FUNCTION_TEST( Round_f32, float, FS_Store_f32( &result, FS_Round_f32( FS_Min_f32( FS::float32v( MAX_ROUNDING ), FS_Max_f32( FS::float32v( -MAX_ROUNDING ), FS_Load_f32( &rndFloats0[i] ) ) ) ) ) )

SIMD_FUNCTION_TEST( Add_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) + FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Sub_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) - FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Mul_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) * FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Div_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) / FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( And_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) & FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Xor_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) ^ FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Or_f32, float, FS_Store_f32( &result, FS_Load_f32( &rndFloats0[i] ) | FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Not_f32, float, FS_Store_f32( &result, ~FS_Load_f32( &rndFloats1[i] ) ) )
SIMD_FUNCTION_TEST( Negate_f32, float, FS_Store_f32( &result, -FS_Load_f32( &rndFloats1[i] ) ) )

SIMD_FUNCTION_TEST( Add_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) + FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Sub_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) - FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Mul_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) * FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( And_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) & FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Xor_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) ^ FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Or_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) | FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Not_i32, int32_t, FS_Store_i32( &result, ~FS_Load_i32( &rndInts1[i] ) ) )
SIMD_FUNCTION_TEST( Negate_i32, int32_t, FS_Store_i32( &result, -FS_Load_i32( &rndInts1[i] ) ) )

SIMD_FUNCTION_TEST( ShiftL_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) << (rndInts1[i & NonVecMask] & 31) ) )
SIMD_FUNCTION_TEST( ShiftR_i32, int32_t, FS_Store_i32( &result, FS_Load_i32( &rndInts0[i] ) >> (rndInts1[i & NonVecMask] & 31) ) )


int main( int argc, char** argv )
{
    std::cout << std::fixed;

    SIMDUnitTest::RunAll();

    std::cout << "Tests Complete!\n";

    getchar();
    return 0;
}
