#pragma once

#include "VecTools.h"
#include <algorithm>

namespace FastSIMD
{

    struct Scalar_Float
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Float, float );

        FS_INLINE static Scalar_Float Incremented()
        {
            return 0.0f;
        }

        FS_INLINE Scalar_Float& operator+=( const Scalar_Float& rhs )
        {
            *this = *this + rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator-=( const Scalar_Float& rhs )
        {
            *this = *this - rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator*=( const Scalar_Float& rhs )
        {
            *this = *this * rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator/=( const Scalar_Float& rhs )
        {
            *this = *this / rhs;
            return *this;
        }
    };

    struct Scalar_Int
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Int, int32_t );

        FS_INLINE static Scalar_Int Incremented()
        {
            return 0;
        }

        FS_INLINE Scalar_Int& operator+=( const Scalar_Int& rhs )
        {
            *this = *this + rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator-=( const Scalar_Int& rhs )
        {
            *this = *this - rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator*=( const Scalar_Int& rhs )
        {
            *this = *this * rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator/=( const Scalar_Int& rhs )
        {
            *this = *this / rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator&=( const Scalar_Int& rhs )
        {
            *this = *this & rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator|=( const Scalar_Int& rhs )
        {
            *this = *this | rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator^=( const Scalar_Int& rhs )
        {
            *this = *this ^ rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator>>=( int32_t rhs )
        {
            *this = *this >> rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator<<=( int32_t rhs )
        {
            *this = *this << rhs;
            return *this;
        }
    };

    struct Scalar_Mask
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Mask, bool );

        FS_INLINE Scalar_Mask operator~() const
        {
            return !(*this);
        }
    };

    class Scalar
    {
    public:
        static constexpr eLevel SIMD_Level = FastSIMD::Level_Scalar;

        template<size_t ElementSize = 8>
        static constexpr size_t VectorSize = 32 / ElementSize;

        typedef Scalar_Float float32v;
        typedef Scalar_Int   int32v;
        typedef Scalar_Mask  mask32v;

        // Load

        FS_INLINE static float32v Load_f32( void const* p )
        {
            return *reinterpret_cast<float32v const*>(p);
        }

        FS_INLINE static int32v Load_i32( void const* p )
        {
            return *reinterpret_cast<int32v const*>(p);
        }

        // Store

        FS_INLINE static void Store_f32( void* p, float32v a )
        {
            *reinterpret_cast<float32v*>(p) = a;
        }

        FS_INLINE static void Store_i32( void* p, int32v a )
        {
            *reinterpret_cast<int32v*>(p) = a;
        }

        // Cast

        FS_INLINE static float32v Casti32_f32( int32v a )
        {
            union
            {
                int32_t i;
                float   f;
            } u;

            u.i = a;
            return u.f;
        }

        FS_INLINE static int32v Castf32_i32( float32v a )
        {
            union
            {
                int32_t i;
                float   f;
            } u;

            u.f = a;
            return u.i;
        }

        // Convert

        FS_INLINE static float32v Converti32_f32( int32v a )
        {
            return static_cast<float>(a);
        }

        FS_INLINE static int32v Convertf32_i32( float32v a )
        {
            return static_cast<int32_t>(nearbyint( a ));
        }

        // Comparisons

        FS_INLINE static mask32v Equal_f32( float32v a, float32v b )
        {
            return a == b;
        }

        FS_INLINE static mask32v GreaterThan_f32( float32v a, float32v b )
        {
            return a > b;
        }

        FS_INLINE static mask32v LessThan_f32( float32v a, float32v b )
        {
            return a < b;
        }

        FS_INLINE static mask32v GreaterEqualThan_f32( float32v a, float32v b )
        {
            return a >= b;
        }

        FS_INLINE static mask32v LessEqualThan_f32( float32v a, float32v b )
        {
            return a <= b;
        }

        FS_INLINE static mask32v Equal_i32( int32v a, int32v b )
        {
            return a == b;
        }

        FS_INLINE static mask32v GreaterThan_i32( int32v a, int32v b )
        {
            return a > b;
        }

        FS_INLINE static mask32v LessThan_i32( int32v a, int32v b )
        {
            return a < b;
        }

        // Select

        FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
        {
            return m ? a : b;
        }

        FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
        {
            return m ? a : b;
        }

        // Min, Max

        FS_INLINE static float32v Min_f32( float32v a, float32v b )
        {
            return fminf( a, b );
        }

        FS_INLINE static float32v Max_f32( float32v a, float32v b )
        {
            return fmaxf( a, b );
        }

        FS_INLINE static int32v Min_i32( int32v a, int32v b )
        {
            return std::min( a, b );
        }

        FS_INLINE static int32v Max_i32( int32v a, int32v b )
        {
            return std::max( a, b );
        }

        // Bitwise

        FS_INLINE static float32v BitwiseAnd_f32( float32v a, float32v b )
        {
            return Casti32_f32( Castf32_i32( a ) & Castf32_i32( b ) );
        }

        FS_INLINE static float32v BitwiseOr_f32( float32v a, float32v b )
        {
            return Casti32_f32( Castf32_i32( a ) | Castf32_i32( b ) );
        }

        FS_INLINE static float32v BitwiseXor_f32( float32v a, float32v b )
        {
            return Casti32_f32( Castf32_i32( a ) ^ Castf32_i32( b ) );
        }

        FS_INLINE static float32v BitwiseNot_f32( float32v a )
        {
            return Casti32_f32( ~Castf32_i32( a ) );
        }

        FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
        {
            return Casti32_f32( Castf32_i32( a ) & ~Castf32_i32( b ) );
        }

        FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
        {
            return a & ~b;
        }

        // Abs

        FS_INLINE static float32v Abs_f32( float32v a )
        {
            return fabsf( a );
        }

        FS_INLINE static int32v Abs_i32( int32v a )
        {
            return abs( a );
        }

        // Float math

        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return sqrtf( a );
        }

        FS_INLINE static float32v InvSqrt_f32( float32v a )
        {
            float xhalf = 0.5f * a;
            a = Casti32_f32( 0x5f3759df - (Castf32_i32( a ) >> 1) );
            a = a * (1.5f - xhalf * a * a);
            return a;
        }

        FS_INLINE static float32v Reciprocal_f32( float32v a )
        {
            // pow( pow(x,-0.5), 2 ) = pow( x, -1 ) = 1.0 / x
            a = Casti32_f32( (0xbe6eb3beU - Castf32_i32( a )) >> 1 );
            return a * a;
        }

        // Floor, Ceil, Round

        FS_INLINE static float32v Floor_f32( float32v a )
        {
            return floorf( a );
        }

        FS_INLINE static float32v Ceil_f32( float32v a )
        {
            return ceilf( a );
        }

        FS_INLINE static float32v Round_f32( float32v a )
        {
            return nearbyintf( a );
        }

        // Mask

        FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
        {
            return m ? a : int32v(0);
        }

        FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
        {
            return m ? a : float32v(0);
        }

        FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
        {
            return m ? int32v(0) : a;
        }

        FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
        {
            return m ? float32v(0) : a;
        }
    };
}
