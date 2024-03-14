#pragma once


#include <wasm_simd128.h>

#include "VecTools.h"


namespace FastSIMD
{

    struct WASM_i32x4
    {
        FASTSIMD_INTERNAL_TYPE_SET( WASM_i32x4, v128_t );


        FS_INLINE static WASM_i32x4 Zero()
        {
            return wasm_i32x4_const_splat( 0 );
        }

        FS_INLINE static WASM_i32x4 Incremented()
        {
            return wasm_i32x4_const( 0, 1, 2, 3 );
        }

        FS_INLINE explicit WASM_i32x4( int32_t i )
        {
            *this = wasm_i32x4_const_splat( i );
        }

        FS_INLINE explicit WASM_i32x4( int32_t i0, int32_t i1, int32_t i2, int32_t i3 )
        {
            *this = wasm_i32x4_const( i0, i1, i2, i3 );
        }

        FS_INLINE WASM_i32x4& operator+=( const WASM_i32x4& rhs )
        {
            *this = wasm_i32x4_add( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator-=( const WASM_i32x4& rhs )
        {
            *this = wasm_i32x4_sub( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator*=( const WASM_i32x4& rhs )
        {
            *this = wasm_i32x4_mul( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator&=( const WASM_i32x4& rhs )
        {
            *this = wasm_v128_and( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator|=( const WASM_i32x4& rhs )
        {
            *this = wasm_v128_or( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator^=( const WASM_i32x4& rhs )
        {
            *this = wasm_v128_xor( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_i32x4& operator>>=( const int32_t rhs )
        {
            *this = wasm_i32x4_shr(*this, rhs);
            return *this;
        }

        FS_INLINE WASM_i32x4& operator<<=( const int32_t rhs )
        {
            *this = wasm_i32x4_shr(*this, rhs);//use shift left by constant for faster execution
            return *this;
        }

        FS_INLINE WASM_i32x4 operator~() const
        {
            return wasm_v128_not( *this );
        }

        FS_INLINE WASM_i32x4 operator-() const
        {
            return wasm_i32x4_neg( *this );
        }

        FS_INLINE WASM_i32x4 operator<( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_lt( *this, b );
        }

        FS_INLINE WASM_i32x4 operator>( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_gt( *this, b );
        }

        FS_INLINE WASM_i32x4 operator<=( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_le( *this, b );
        }

        FS_INLINE WASM_i32x4 operator>=( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_ge( *this, b );
        }

        FS_INLINE WASM_i32x4 operator!=( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_ne( *this, b );
        }

        FS_INLINE WASM_i32x4 operator==( const WASM_i32x4 &b ) const
        {
            return wasm_i32x4_eq( *this, b );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_INT( WASM_i32x4, int32_t )


    struct WASM_f32x4
    {
        FASTSIMD_INTERNAL_TYPE_SET( WASM_f32x4, v128_t );


        FS_INLINE static WASM_f32x4 Zero()
        {
            return wasm_f32x4_const_splat( 0 );
        }

        FS_INLINE static WASM_f32x4 Incremented()
        {
            return wasm_f32x4_const( 0.0f, 1.0f, 2.0f, 3.0f );
        }

        FS_INLINE explicit WASM_f32x4( float f )
        {
            *this = wasm_f32x4_const_splat( f );
        }

        FS_INLINE explicit WASM_f32x4( float f0, float f1, float f2, float f3 )
        {
            *this = wasm_f32x4_const( f0, f1, f2, f3 );
        }

        FS_INLINE WASM_f32x4& operator+=( const WASM_f32x4& rhs )
        {
            *this = wasm_f32x4_add( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator-=( const WASM_f32x4& rhs )
        {
            *this = wasm_f32x4_sub( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator*=( const WASM_f32x4& rhs )
        {
            *this = wasm_f32x4_mul( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator/=( const WASM_f32x4& rhs )
        {
            *this = wasm_f32x4_div( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator&=( const WASM_f32x4& rhs )
        {
            *this = wasm_v128_and( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator|=( const WASM_f32x4& rhs )
        {
            *this = wasm_v128_or( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4& operator^=( const WASM_f32x4& rhs )
        {
            *this = wasm_v128_xor( *this, rhs );
            return *this;
        }

        FS_INLINE WASM_f32x4 operator-() const
        {
            return wasm_f32x4_neg( *this );
        }

        FS_INLINE WASM_f32x4 operator~() const
        {
            return wasm_v128_not(*this);
        }

        FS_INLINE WASM_i32x4 operator<( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_lt( *this, b );
        }

        FS_INLINE WASM_i32x4 operator>( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_gt( *this, b );
        }

        FS_INLINE WASM_i32x4 operator<=( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_le( *this, b );
        }

        FS_INLINE WASM_i32x4 operator>=( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_ge( *this, b );
        }

        FS_INLINE WASM_i32x4 operator!=( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_ne( *this, b );
        }

        FS_INLINE WASM_i32x4 operator==( const WASM_f32x4 &b ) const
        {
            return wasm_f32x4_eq( *this, b );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_FLOAT( WASM_f32x4 )





    template<eLevel LEVEL_T>
    class WASM_T
    {
    public:
        static constexpr eLevel SIMD_Level = FastSIMD::Level_WASM;


        template<size_t ElementSize>
        static constexpr size_t VectorSize = (128 / 8) / ElementSize;

        typedef WASM_f32x4 float32v;
        typedef WASM_i32x4   int32v;
        typedef WASM_i32x4  mask32v;

        FS_INLINE static float32v Load_f32( void const* p )
        {
            return vld1q_f32( reinterpret_cast<float const*>(p) );
        }

        FS_INLINE static int32v Load_i32( void const* p )
        {
            return vld1q_s32( reinterpret_cast<int32_t const*>(p) );
        }

        // Store

        FS_INLINE static void Store_f32( void* p, float32v a )
        {
            vst1q_f32( reinterpret_cast<float*>(p), a );
        }

        FS_INLINE static void Store_i32( void* p, int32v a )
        {
            vst1q_s32( reinterpret_cast<int32_t*>(p), a );
        }

        // Cast

        FS_INLINE static float32v Casti32_f32( int32v a )
        {
            return vreinterpretq_f32_s32( a );
        }

        FS_INLINE static int32v Castf32_i32( float32v a )
        {
            return vreinterpretq_s32_f32( a );
        }

        // Convert

        FS_INLINE static float32v Converti32_f32( int32v a )
        {
            return vcvtq_f32_s32( a );
        }

        FS_INLINE static int32v Convertf32_i32( float32v a )
        {
            return vcvtq_s32_f32( Round_f32(a) );
        }

        // Comparisons

        FS_INLINE static mask32v Equal_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vceqq_f32( a, b ) );
        }

        FS_INLINE static mask32v GreaterThan_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vcgtq_f32( a, b ) );
        }

        FS_INLINE static mask32v LessThan_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vcltq_f32( a, b ) );
        }

        FS_INLINE static mask32v GreaterEqualThan_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vcgeq_f32( a, b ) );
        }

        FS_INLINE static mask32v LessEqualThan_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vcleq_f32( a, b ) );
        }

        FS_INLINE static mask32v Equal_i32( int32v a, int32v b )
        {
            return vreinterpretq_s32_u32( vceqq_s32( a, b ) );
        }

        FS_INLINE static mask32v GreaterThan_i32( int32v a, int32v b )
        {
            return vreinterpretq_s32_u32( vcgtq_s32( a, b ) );
        }

        FS_INLINE static mask32v LessThan_i32( int32v a, int32v b )
        {
            return vreinterpretq_s32_u32( vcltq_s32( a, b ) );
        }

        // Select

        FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
        {
            return vbslq_f32( vreinterpretq_u32_s32( m ), a, b );
        }
        FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
        {
            return vbslq_s32( vreinterpretq_u32_s32( m ), a, b );
        }

        // Min, Max

        FS_INLINE static float32v Min_f32( float32v a, float32v b )
        {
            return vminq_f32( a, b );
        }

        FS_INLINE static float32v Max_f32( float32v a, float32v b )
        {
            return vmaxq_f32( a, b );
        }

        FS_INLINE static int32v Min_i32( int32v a, int32v b )
        {
            return vminq_s32( a, b );
        }

        FS_INLINE static int32v Max_i32( int32v a, int32v b )
        {
            return vmaxq_s32( a, b );
        }

        // Bitwise

        FS_INLINE static float32v BitwiseAnd_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( vandq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }
/*
        FS_INLINE static float32v BitwiseOr_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( vorrq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }

        FS_INLINE static float32v BitwiseXor_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( veorq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }

        FS_INLINE static float32v BitwiseNot_f32( float32v a )
        {
            return vreinterpretq_f32_u32( vmvnq_u32( vreinterpretq_u32_f32( a ) ) );
        }
*/
        FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( vandq_u32( vreinterpretq_u32_f32( a ), vmvnq_u32( vreinterpretq_u32_f32( b ) ) ) );
        }

        FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
        {
            return vandq_s32( a , vmvnq_s32( b ) );
        }

        // Abs

        FS_INLINE static float32v Abs_f32( float32v a )
        {
            return vabsq_f32( a );
        }

        FS_INLINE static int32v Abs_i32( int32v a )
        {
            return vabsq_s32( a );
        }

        FS_INLINE static float32v InvSqrt_f32( float32v a )
        {
            return vrsqrteq_f32( a );
        }

        // Floor, Ceil, Round:

        FS_INLINE static float32v Floor_f32( float32v a )
        {
            return vrndmq_f32( a );
        }

        FS_INLINE static float32v Ceil_f32( float32v a )
        {
            return vrndpq_f32( a );
        }

        FS_INLINE static float32v Round_f32( float32v a )
        {
            return vrndnq_f32( a );
        }

        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return vsqrtq_f32( a );
        }

        // Mask

        FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
        {
            return a & m;
        }

        FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
        {
            return BitwiseAndNot_i32(a, m);
        }

        FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
        {
            return BitwiseAnd_f32( a, vreinterpretq_f32_s32( m ) );
        }

        FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
        {
            return BitwiseAndNot_f32( a, vreinterpretq_f32_s32( m ) );
        }

        FS_INLINE static float Extract0_f32( float32v a )
        {
            return vgetq_lane_f32(a, 0);
        }

        FS_INLINE static int32_t Extract0_i32( int32v a )
        {
            return vgetq_lane_s32(a, 0);
        }

        FS_INLINE static float32v Reciprocal_f32( float32v a )
        {
            return vrecpeq_f32( a );
        }

        FS_INLINE static float32v BitwiseShiftRightZX_f32( float32v a, int32_t b )
        {
            int32x4_t rhs2 = vdupq_n_s32( -b );
            return vreinterpretq_f32_u32 ( vshlq_u32( vreinterpretq_u32_f32(a), rhs2) );
        }

        FS_INLINE static int32v BitwiseShiftRightZX_i32( int32v a, int32_t b )
        {
            int32x4_t rhs2 = vdupq_n_s32( -b );
            return vreinterpretq_s32_u32 (vshlq_u32( vreinterpretq_u32_s32(a), rhs2));
        }
        FS_INLINE static bool AnyMask_bool( mask32v m )
        {
            uint32x2_t tmp = vorr_u32(vget_low_u32(vreinterpretq_u32_s32(m)), vget_high_u32(vreinterpretq_u32_s32(m)));
            return vget_lane_u32(vpmax_u32(tmp, tmp), 0);
        }
    };

#if FASTSIMD_COMPILE_WASM
    typedef WASM_T<Level_WASM> WASM;
#endif
}
