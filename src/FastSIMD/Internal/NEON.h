#pragma once

#include <arm_neon.h>

#include "VecTools.h"

struct NEON_f32x4
{
    FASTSIMD_INTERNAL_TYPE_SET( NEON_f32x4, float32x4_t );

    constexpr FS_INLINE static uint8_t Size()
    {
        return 4;
    }

    FS_INLINE static NEON_f32x4 Zero()
    {
        return vdupq_n_f32( 0 );
    }

    FS_INLINE static NEON_f32x4 Incremented()
    {
        alignas(16) const float f[4]{ 0.0f, 1.0f, 2.0f, 3.0f };
        return vld1q_f32( f );
    }

    FS_INLINE explicit NEON_f32x4( float f )
    {
        *this = vdupq_n_f32( f );
    }

    FS_INLINE explicit NEON_f32x4( float f0, float f1, float f2, float f3 )
    {
        alignas(16) const float f[4]{ f0, f1, f2, f3 };
        *this = vld1q_f32( f );
    }

    FS_INLINE NEON_f32x4& operator+=( const NEON_f32x4& rhs )
    {
        *this = vaddq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator-=( const NEON_f32x4& rhs )
    {
        *this = vsubq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator*=( const NEON_f32x4& rhs )
    {
        *this = vmulq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator/=( const NEON_f32x4& rhs )
    {
#if defined(__aarch64__)
        *this = vdivq_f32( *this, rhs );
#else
        float32x4_t reciprocal = vrecpeq_f32( rhs );
        // use a couple Newton-Raphson steps to refine the estimate.  Depending on your
        // application's accuracy requirements, you may be able to get away with only
        // one refinement (instead of the two used here).  Be sure to test!
        reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );
        reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );

        // and finally, compute a/b = a*(1/b)
        *this = vmulq_f32( *this, reciprocal );
#endif
        return *this;
    }
    
    FS_INLINE NEON_f32x4& operator&=( const NEON_f32x4& rhs )
    {
        *this = vreinterpretq_f32_u32( vandq_u32( vreinterpretq_u32_f32( *this ), vreinterpretq_u32_f32( rhs ) ) );
        return *this;
    }
    
    FS_INLINE NEON_f32x4& operator|=( const NEON_f32x4& rhs )
    {
        *this = vreinterpretq_f32_u32( vorrq_u32( vreinterpretq_u32_f32( *this ), vreinterpretq_u32_f32( rhs ) ) );
        return *this;
    }
    
    FS_INLINE NEON_f32x4& operator^=( const NEON_f32x4& rhs )
    {
        *this = vreinterpretq_f32_u32( veorq_u32( vreinterpretq_u32_f32( *this ), vreinterpretq_u32_f32( rhs ) ) );
        return *this;
    }
    
    FS_INLINE NEON_f32x4 operator~() const
    {
        return vreinterpretq_f32_u32( vmvnq_u32( vreinterpretq_u32_f32( *this ) ) );
    }

    FS_INLINE NEON_f32x4 operator-() const
    {
        return vnegq_f32( *this );
    }
};

FASTSIMD_INTERNAL_OPERATORS_FLOAT( NEON_f32x4 )


struct NEON_i32x4
{
    FASTSIMD_INTERNAL_TYPE_SET( NEON_i32x4, int32x4_t );

    constexpr FS_INLINE static uint8_t Size()
    {
        return 4;
    }

    FS_INLINE static NEON_i32x4 Zero()
    {
        return vdupq_n_s32( 0 );
    }

    FS_INLINE static NEON_i32x4 Incremented()
    {
        alignas(16) const int32_t f[4]{ 0, 1, 2, 3 };
        return vld1q_s32( f );
    }

    FS_INLINE explicit NEON_i32x4( int32_t i )
    {
        *this = vdupq_n_s32( i );
    }

    FS_INLINE explicit NEON_i32x4( int32_t i0, int32_t i1, int32_t i2, int32_t i3 )
    {
        alignas(16) const int32_t f[4]{ i0, i1, i2, i3 };
        *this = vld1q_s32( f );
    }

    FS_INLINE NEON_i32x4& operator+=( const NEON_i32x4& rhs )
    {
        *this = vaddq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator-=( const NEON_i32x4& rhs )
    {
        *this = vsubq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator*=( const NEON_i32x4& rhs )
    {
        *this = vmulq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator&=( const NEON_i32x4& rhs )
    {
        *this = vandq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator|=( const NEON_i32x4& rhs )
    {
        *this = vorrq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator^=( const NEON_i32x4& rhs )
    {
        *this = veorq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator>>=( const int32_t rhs )
    {
        *this = vshlq_s32( *this, vdupq_n_s32( -rhs ) );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator<<=( const int32_t rhs )
    {
        *this = vshlq_s32( *this, vdupq_n_s32( rhs ) );
        return *this;
    }

    FS_INLINE NEON_i32x4 operator~() const
    {
        return vmvnq_s32( *this );
    }

    FS_INLINE NEON_i32x4 operator-() const
    {
        return vnegq_s32( *this );
    }
};

FASTSIMD_INTERNAL_OPERATORS_INT( NEON_i32x4, int32_t )

template<FastSIMD::eLevel LEVEL_T>
class FastSIMD_NEON_T
{
public:
    static const FastSIMD::eLevel SIMD_Level = LEVEL_T;
    
    template<size_t ElementSize>
    static constexpr size_t VectorSize = (128 / 8) / ElementSize;

    typedef NEON_f32x4 float32v;
    typedef NEON_i32x4 int32v;
    typedef NEON_i32x4 mask32v;

    // Load

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
    
    // Extract
    
    FS_INLINE static float Extract0_f32( float32v a )
    {
        return vgetq_lane_f32( a, 0 );
    }
    
    FS_INLINE static int32_t Extract0_i32( int32v a )
    {
        return vgetq_lane_s32( a, 0 );
    }
    
    FS_INLINE static float Extract_f32( float32v a, size_t idx )
    {
        switch ( idx & 3 ) {
            default:
            case 0:
                return vgetq_lane_f32( a, 0 );
            case 1:
                return vgetq_lane_f32( a, 1 );
            case 2:
                return vgetq_lane_f32( a, 2 );
            case 3:
                return vgetq_lane_f32( a, 3 );
        }
    }

    FS_INLINE static int32_t Extract_i32( int32v a, size_t idx )
    {
        switch ( idx & 3 ) {
            default:
            case 0:
                return vgetq_lane_s32( a, 0 );
            case 1:
                return vgetq_lane_s32( a, 1 );
            case 2:
                return vgetq_lane_s32( a, 2 );
            case 3:
                return vgetq_lane_s32( a, 3 );
        }
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
        return vcvtq_s32_f32( a );
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
        return vreinterpretq_f32_s32( vandq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseOr_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( vorrq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseXor_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( veorq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseNot_f32( float32v a )
    {
        return vreinterpretq_f32_s32( vmvnq_s32( vreinterpretq_s32_f32( a ) ) );
    }

    FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( vandq_s32( vreinterpretq_s32_f32( a ), vmvnq_s32( vreinterpretq_s32_f32( b ) ) ) );
    }

    FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
    {
        return vandq_s32( a , vmvnq_s32( b ) );
    }
    
    FS_INLINE static int32v BitwiseShiftRightZX_i32( int32v a, int32_t b )
    {
        return vshlq_s32( a, vdupq_n_s32( -b ) );
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

    // Float math

    FS_INLINE static float32v Sqrt_f32( float32v a )
    {
#if defined(__aarch64__)
        return vsqrtq_f32( a );
#else
        float32x4_t reciprocal = vrsqrteq_f32( a );
        reciprocal = vmulq_f32( vrsqrtsq_f32( vmulq_f32( a, reciprocal ), reciprocal ), reciprocal );
        reciprocal = vmulq_f32( vrsqrtsq_f32( vmulq_f32( a, reciprocal ), reciprocal ), reciprocal );
        return vmulq_f32( a, reciprocal );
#endif
    }

    FS_INLINE static float32v InvSqrt_f32( float32v a )
    {
        float32x4_t reciprocal = vrsqrteq_f32( a );
        reciprocal = vmulq_f32( vrsqrtsq_f32( vmulq_f32( a, reciprocal ), reciprocal ), reciprocal );
        reciprocal = vmulq_f32( vrsqrtsq_f32( vmulq_f32( a, reciprocal ), reciprocal ), reciprocal );
        return reciprocal;
    }
    
    FS_INLINE static float32v Reciprocal_f32( float32v a )
    {
        float32v reciprocal = vrecpeq_f32( a );
        reciprocal = vmulq_f32( vrecpsq_f32( a, reciprocal ), reciprocal );
        reciprocal = vmulq_f32( vrecpsq_f32( a, reciprocal ), reciprocal );
        return reciprocal;
    }

    // Floor, Ceil, Round: http://dss.stephanierct.com/DevBlog/?p=8

    FS_INLINE static float32v Floor_f32( float32v a )
    {
#if defined(__aarch64__)
        return vrndmq_f32( a );
#else
        const float32v f1 = vdupq_n_f32( 1.0f );
        float32v fval = vcvtq_f32_s32( vcvtq_s32_f32( a ) );
        return vsubq_f32( fval, BitwiseAnd_f32( vreinterpretq_f32_u32( vcgtq_f32( fval, a ) ), f1 ) );
#endif
    }

    FS_INLINE static float32v Ceil_f32( float32v a )
    {
#if defined(__aarch64__)
        return vrndpq_f32( a );
#else
        const float32v f1 = vdupq_n_f32( 1.0f );
        float32v fval = vcvtq_f32_s32( vcvtq_s32_f32( a ) );
        return vaddq_f32( fval, BitwiseAnd_f32( vreinterpretq_f32_u32( vcltq_f32( fval, a ) ), f1 ) );
#endif
    }

    FS_INLINE static float32v Round_f32( float32v a )
    {
#if defined(__aarch64__)
        return vrndxq_f32( a );
#else
        const float32v f2 = vdupq_n_f32( 2.f );
        float32x4_t aTrunc = vcvtq_f32_s32( vcvtq_s32_f32( a ) );
        float32x4_t rmd = vsubq_f32( a, aTrunc );
        float32x4_t rmd2 = vmulq_f32( rmd, f2 );
        float32x4_t rmd2Trunc = vcvtq_f32_s32( vcvtq_s32_f32( rmd2 ) );
        return vaddq_f32( aTrunc, rmd2Trunc );
#endif
    }

    // Mask

    FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
    {
        return vandq_s32( a, m );
    }

    FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
    {
        return BitwiseAnd_f32( a, vreinterpretq_f32_s32( m ) );
    }
    
    FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
    {
        return vandq_s32( a, vmvnq_s32( m ) );
    }
    
    FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
    {
        return BitwiseAndNot_f32( a, vreinterpretq_f32_s32( m ) );
    }
    
    FS_INLINE static bool AnyMask_bool( mask32v m )
    {
        int64x2_t mAnd = vandq_s64( vreinterpretq_s64_s32( m ), vreinterpretq_s64_s32( m ) );
        return vgetq_lane_s64( mAnd, 0 ) | vgetq_lane_s64( mAnd, 1 );
    }
};

#if FASTSIMD_COMPILE_NEON
namespace FastSIMD
{
    typedef FastSIMD_NEON_T<FastSIMD::Level_NEON> NEON;
}
#endif
