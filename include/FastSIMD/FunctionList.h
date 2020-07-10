#pragma once
#include <inttypes.h>
#include <type_traits>
#include <memory>

#include "FastSIMD/FastSIMD.h"

#ifdef _MSC_VER
#define FS_VECTORCALL __vectorcall
#define FS_INLINE __forceinline
#else
#define FS_VECTORCALL 
#define FS_INLINE __attribute__((always_inline))
#endif

#define FS_ENABLE_IF( CONDITION, TYPE ) typename std::enable_if<(CONDITION), TYPE >::type 

#define FS_MULTI_SPECIALISATION( ... ) FastSIMD::MultiSpecialisation<FS_SIMD_CLASS::SIMD_Level, __VA_ARGS__ >::Level

// Vector builders

// Compile time constant
// returns: Number of float32 that will fit into the vector
// I
// int FS_VectorSize_f32()
#define FS_Size() Size()

// returns: Vector with all values set to 0
// I
// int32v FS_VecZero_i32()
#define FS_Zero() Zero()

// returns: Vector with values incrementing from 0 based on index {0, 1, 2, 3...}
// I
// int32v FS_VecIncremented_i32()
#define FS_Incremented() Incremented()


// Load

// Copies {vectorSize} bytes from memory location into a vector
// ptr: Pointer to first byte (unaligned)
// returns: Vector with copied data
// I
// float32v FS_Store_f32( void const* ptr )
#define FS_Load_f32( ... ) FS::Load_f32( __VA_ARGS__ )

// Copies {vectorSize} bytes from memory location into a vector
// ptr: Pointer to first byte (unaligned)
// returns: Vector with copied data
// I
// int32v FS_Load_i32( void const* ptr )
#define FS_Load_i32( ... ) FS::Load_i32( __VA_ARGS__ )


// Store

// Copies all elements of a vector to given memory location
// ptr: Pointer to memory location that elements will be copied to
// vec: Vector to copy from
// I
// void FS_Store_f32( void* ptr, float32v vec )
#define FS_Store_f32( ... ) FS::Store_f32( __VA_ARGS__ )

// Copies all elements of a vector to given memory location
// ptr: Pointer to memory location that elements will be copied to
// vec: Vector to copy from
// I
// void FS_Store_i32( void* ptr, int32v vec )
#define FS_Store_i32( ... ) FS::Store_i32( __VA_ARGS__ )


// Cast

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_Casti32_f32( int32v a )
#define FS_Casti32_f32( ... ) FS::Casti32_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_Castf32_i32( float32v a )
#define FS_Castf32_i32( ... ) FS::Castf32_i32( __VA_ARGS__ )


// Convert

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_Converti32_f32( int32v a )
#define FS_Converti32_f32( ... ) FS::Converti32_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_Convertf32_i32( float32v a )
#define FS_Convertf32_i32( ... ) FS::Convertf32_i32( __VA_ARGS__ )


// Comparisons

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_Equal_f32( float32v a, float32v b )
#define FS_Equal_f32( ... ) FS::Equal_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_GreaterThan_f32( float32v a, float32v b )
#define FS_GreaterThan_f32( ... ) FS::GreaterThan_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_LessThan_f32( float32v a, float32v b )
#define FS_LessThan_f32( ... ) FS::LessThan_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_GreaterEqualThan_f32( float32v a, float32v b )
#define FS_GreaterEqualThan_f32( ... ) FS::GreaterEqualThan_f32( __VA_ARGS__ ) 

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_LessEqualThan_f32( float32v a, float32v b )
#define FS_LessEqualThan_f32( ... ) FS::LessEqualThan_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_Equal_i32( int32v a, int32v b )
#define FS_Equal_i32( ... ) FS::Equal_i32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_GreaterThan_i32( int32v a, int32v b )
#define FS_GreaterThan_i32( ... ) FS::GreaterThan_i32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_LessThan_i32( int32v a, int32v b )
#define FS_LessThan_i32( ... ) FS::LessThan_i32( __VA_ARGS__ )


// Select

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_Select_f32( mask32v m, float32v a, float32v b )
#define FS_Select_f32( ... ) FS::Select_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_Select_i32( mask32v m, int32v a, int32v b )
#define FS_Select_i32( ... ) FS::Select_i32( __VA_ARGS__ )


// Min, Max

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_Min_f32( float32v a, float32v b )
#define FS_Min_f32( ... ) FS::Min_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_Max_f32( float32v a, float32v b )
#define FS_Max_f32( ... ) FS::Max_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_Min_i32( int32v a, int32v b )
#define FS_Min_i32( ... ) FS::Min_i32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_Max_i32( int32v a, int32v b )
#define FS_Max_i32( ... ) FS::Max_i32( __VA_ARGS__ )


// Bitwise

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_BitwiseAnd_f32( float32v a, float32v b )
#define FS_BitwiseAnd_f32( ... ) FS::BitwiseAnd_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_BitwiseOr_f32( float32v a, float32v b )
#define FS_BitwiseOr_f32( ... ) FS::BitwiseOr_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_BitwiseXor_f32( float32v a, float32v b )
#define FS_BitwiseXor_f32( ... ) FS::BitwiseXor_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_BitwiseNot_f32( float32v a )
#define FS_BitwiseNot_f32( ... ) FS::BitwiseNot_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// float32v FS_BitwiseAndNot_f32( float32v a, float32v b )
#define FS_BitwiseAndNot_f32( ... ) FS::BitwiseAndNot_f32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// int32v FS_BitwiseAndNot_i32( int32v a, int32v b )
#define FS_BitwiseAndNot_i32( ... ) FS::BitwiseAndNot_i32( __VA_ARGS__ )

// TEXT
// val: TEXT
// returns: TEXT
// I
// mask32v FS_BitwiseAndNot_m32( mask32v a, mask32v b )
#define FS_BitwiseAndNot_m32( ... ) FastSIMD::BitwiseAndNot_m32<FS>( __VA_ARGS__ )


// Abs

// I
// float32v FS_Abs_f32( float32v a )
// returns: abs(a)
#define FS_Abs_f32( ... ) FS::Abs_f32( __VA_ARGS__ )

// I
// int32v FS_Abs_i32( int32v a )
// returns: abs(a)
#define FS_Abs_i32( ... ) FS::Abs_i32( __VA_ARGS__ )

// Float math

// I
// float32v FS_Sqrt_f32( float32v a )
// returns: sqrt(a)
#define FS_Sqrt_f32( ... ) FS::Sqrt_f32( __VA_ARGS__ )

// I
// float32v FS_InvSqrt_f32( float32v a )
// returns: APPROXIMATE( 1.0 / sqrt(a) )
#define FS_InvSqrt_f32( ... ) FS::InvSqrt_f32( __VA_ARGS__ )

// I
// float32v FS_Reciprocal_f32( float32v a )
// returns: APPROXIMATE( 1.0 / a )
#define FS_Reciprocal_f32( ... ) FS::Reciprocal_f32( __VA_ARGS__ )

// Floor, Ceil, Round

// I
// float32v FS_Floor_f32( float32v a )
// returns: floor(a)
#define FS_Floor_f32( ... ) FS::Floor_f32( __VA_ARGS__ )

// I
// float32v FS_Ceil_f32( float32v a )
// returns: ceil(a)
#define FS_Ceil_f32( ... ) FS::Ceil_f32( __VA_ARGS__ )

// I
// float32v FS_Round_f32( float32v a )
// returns: round(a)
#define FS_Round_f32( ... ) FS::Round_f32( __VA_ARGS__ )


// Mask

// I
// int32v FS_Mask_i32( int32v a, mask32v m )
// returns: m ? a : 0
#define FS_Mask_i32( ... ) FS::Mask_i32( __VA_ARGS__ )

// I
// float32v FS_Mask_f32( float32v a, mask32v m )
// returns: m ? a : 0
#define FS_Mask_f32( ... ) FS::Mask_f32( __VA_ARGS__ )


// FMA

// I
// float32v FS_FMulAdd_f32( float32v a, float32v b, float32v c )
// returns: (a * b) + c
#define FS_FMulAdd_f32( ... ) FastSIMD::FMulAdd_f32<FS>( __VA_ARGS__ )

// I
// float32v FS_FNMulAdd_f32( float32v a, float32v b, float32v c )
// returns: -(a * b) + c
#define FS_FNMulAdd_f32( ... ) FastSIMD::FNMulAdd_f32<FS>( __VA_ARGS__ )


// Masked float

// I
// float32v FS_MaskedAdd_f32( float32v a, float32v b, mask32v m )
// returns: m ? (a + b) : a
#define FS_MaskedAdd_f32( ... ) FastSIMD::MaskedAdd_f32<FS>( __VA_ARGS__ )

// I
// float32v FS_MaskedSub_f32( float32v a, float32v b, mask32v m )
// returns: m ? (a - b) : a
#define FS_MaskedSub_f32( ... ) FastSIMD::MaskedSub_f32<FS>( __VA_ARGS__ )

// I
// float32v FS_MaskedMul_f32( float32v a, float32v b, mask32v m )
// returns: m ? (a * b) : a
#define FS_MaskedMul_f32( ... ) FastSIMD::MaskedMul_f32<FS>( __VA_ARGS__ )


// Masked int32

// I
// int32v FS_MaskedAdd_i32( int32v a, int32v b, mask32v m )
// returns: m ? (a = b) : a
#define FS_MaskedAdd_i32( ... ) FastSIMD::MaskedAdd_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_MaskedSub_i32( int32v a, int32v b, mask32v m )
// returns: m ? (a - b) : a
#define FS_MaskedSub_i32( ... ) FastSIMD::MaskedSub_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_MaskedMul_i32( int32v a, int32v b, mask32v m )
// returns: m ? (a * b) : a
#define FS_MaskedMul_i32( ... ) FastSIMD::MaskedMul_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_MaskedIncrement_i32( int32v a, mask32v m )
// returns: m ? (a + 1) : a
#define FS_MaskedIncrement_i32( ... ) FastSIMD::MaskedIncrement_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_MaskedIncrement_i32( int32v a, mask32v m )
// returns: m ? (a - 1) : a
#define FS_MaskedDecrement_i32( ... ) FastSIMD::MaskedDecrement_i32<FS>( __VA_ARGS__ )


// NMasked float

// I
// float32v FS_NMaskedAdd_f32( float32v a, float32v b, mask32v m )
// returns: ~m ? (a + b) : a
#define FS_NMaskedAdd_f32( ... ) FastSIMD::NMaskedAdd_f32<FS>( __VA_ARGS__ )

// I
// float32v FS_NMaskedSub_f32( float32v a, float32v b, mask32v m )
// returns: ~m ? (a - b) : a
#define FS_NMaskedSub_f32( ... ) FastSIMD::NMaskedSub_f32<FS>( __VA_ARGS__ )

// I
// float32v FS_NMaskedMul_f32( float32v a, float32v b, mask32v m )
// returns: ~m ? (a * b) : a
#define FS_NMaskedMul_f32( ... ) FastSIMD::NMaskedMul_f32<FS>( __VA_ARGS__ )


// NMasked int32

// I
// int32v FS_NMaskedAdd_i32( int32v a, int32v b, mask32v m )
// returns: ~m ? (a = b) : a
#define FS_NMaskedAdd_i32( ... ) FastSIMD::NMaskedAdd_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_NMaskedSub_i32( int32v a, int32v b, mask32v m )
// returns: ~m ? (a - b) : a
#define FS_NMaskedSub_i32( ... ) FastSIMD::NMaskedSub_i32<FS>( __VA_ARGS__ )

// I
// int32v FS_NMaskedMul_i32( int32v a, int32v b, mask32v m )
// returns: ~m ? (a * b) : a
#define FS_NMaskedMul_i32( ... ) FastSIMD::NMaskedMul_i32<FS>( __VA_ARGS__ )


namespace FastSIMD
{
    template<typename FS, typename T>
    struct VectorSize
    {
        static_assert((FS::VectorSize) % sizeof( T ) == 0, "Type does not fit into the vector exactly");

        constexpr static int Count()
        {            
            return (FS::VectorSize) / sizeof( T );
        }
    };

    //FMA

    template<typename FS>
    FS_INLINE typename FS::float32v FMulAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::float32v c )
    {
        return (a * b) + c;
    }

    template<typename FS>
    FS_INLINE typename FS::float32v FNMulAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::float32v c )
    {
        return -(a * b) + c;
    }

    // Masked float

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a + FS::Mask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedSub_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a - FS::Mask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedMul_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a * FS::Mask_f32( b, m );
    }

    // Masked int32

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedAdd_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a + FS::Mask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedSub_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a - FS::Mask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedMul_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a * FS::Mask_i32( b, m );
    }

    // NMasked float

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a + FS::NMask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedSub_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a - FS::NMask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedMul_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a * FS::NMask_f32( b, m );
    }

    // NMasked int32

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedAdd_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a + FS::NMask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedSub_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a - FS::NMask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedMul_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a * FS::NMask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::int32v> MaskedIncrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return a - m;
    }

    template<typename FS>
    FS_INLINE std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::int32v> MaskedIncrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return MaskedSub_i32<FS>( a, typename FS::int32v( -1 ), m );
    }

    template<typename FS>
    FS_INLINE std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::int32v> MaskedDecrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return a + m;
    }

    template<typename FS>
    FS_INLINE std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::int32v> MaskedDecrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return MaskedAdd_i32<FS>( a, typename FS::int32v( -1 ), m );
    }

    // Bitwise

    template<typename FS>
    FS_INLINE std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::mask32v> BitwiseAndNot_m32( typename FS::mask32v a, typename FS::mask32v b )
    {
        return FS::BitwiseAndNot_i32( a, b );
    }

    template<typename FS>
    FS_INLINE std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>, typename FS::mask32v> BitwiseAndNot_m32( typename FS::mask32v a, typename FS::mask32v b )
    {
        return a & (~b);
    }
}
