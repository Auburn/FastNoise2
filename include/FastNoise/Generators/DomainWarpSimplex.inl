#include "DomainWarpSimplex.h"
#include "Utils.inl"

// The 3D gradient outer product helper is force inlined where the compiler would call it out of line and pass its float32v
// arguments through memory. GCC doesn't inline it by itself (not declared inline, above its auto-inline limit). MSVC AVX512
// passes float32v by reference to a copy (FastSIMD's 512 bit registers have a copy constructor on MSVC). Clang inlines it
// already. The 2D/3D orthogonal matrix and 2D gradient outer product helpers are small and always force inlined, the 4D
// ones everywhere except MSVC AVX512: inlining them makes SuperSimplex 4D too large for MSVC's inliner (2-3x slower)
#if defined( __GNUC__ ) && !defined( __clang__ )
#define FN_WARP_HELPER FS_FORCEINLINE static void FS_VECTORCALL
#define FN_WARP_HELPER_4D FS_FORCEINLINE static void FS_VECTORCALL
#elif defined( _MSC_VER ) && !defined( __clang__ ) && defined( __AVX512F__ )
#define FN_WARP_HELPER FS_FORCEINLINE static void
#define FN_WARP_HELPER_4D static void FS_VECTORCALL
#else
#define FN_WARP_HELPER static void FS_VECTORCALL
#define FN_WARP_HELPER_4D FS_FORCEINLINE static void
#endif

namespace FastNoise
{
    // The (Super)Simplex warps hash each vertex with HashPrimesHB ( hash * A, no xorshift ) and take every bit they consume
    // from the well mixed top bits 21-31. Left shifts are applied to ( hash >> 21 ) rather than to the product itself, so
    // the compiler can't fold them into extra multiplies. The 3D GradientOuterProduct decode reads low bits and keeps HashPrimes
    template<VectorizationScheme Scheme, int Dim, typename I, typename... P>
    FS_FORCEINLINE static I HashWarpVertex( I seed, P... primedPos )
    {
        if constexpr( Dim == 3 && Scheme == VectorizationScheme::GradientOuterProduct )
        {
            return HashPrimes( seed, primedPos... );
        }
        else
        {
            return HashPrimesHB( seed, primedPos... );
        }
    }

    // Unit gradient ( bits 29-31 ) and unit outer vector ( bits 26-28 ) each from 8 directions at 22.5 + 45k degrees.
    // 3 bit indices need no remapping and fit 8 entry tables
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static void ApplyGradientOuterProductVectorProductSimplex( int32v hash, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY )
    {
        constexpr float kA = 0.9238795325112867f; // cos( 22.5 )
        constexpr float kB = 0.3826834323650898f; // sin( 22.5 )
        float32v gX, gY, vX, vY;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            int32v indexGradient = hash >> 29;
            int32v indexOuterVector = hash >> 26; // permutexvar reads 4 bits, the tables repeat after 8

            gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( kA, kB, -kB, -kA, -kA, -kB, kB, kA, kA, kB, -kB, -kA, -kA, -kB, kB, kA ) );
            gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( kB, kA, kA, kB, -kB, -kA, -kA, -kB, kB, kA, kA, kB, -kB, -kA, -kA, -kB ) );
            vX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( kA, kB, -kB, -kA, -kA, -kB, kB, kA, kA, kB, -kB, -kA, -kA, -kB, kB, kA ) );
            vY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( kB, kA, kA, kB, -kB, -kA, -kA, -kB, kB, kA, kA, kB, -kB, -kA, -kA, -kB ) );
        }
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) || kLatticeBlockTbl<SIMD> ) // 8 entry table lookups: vpermps / AArch64 TBL
        {
            int32v indexGradient = hash >> 29;
            int32v indexOuterVector = hash >> 26; // vpermps only reads the low 3 bits
            if constexpr( !( SIMD & FastSIMD::FeatureFlag::AVX2 ) )
            {
                // AArch64 TBL reads 0 past the table
                indexGradient &= int32v( 7 );
                indexOuterVector &= int32v( 7 );
            }

            gX = FN_PermuteTable8( FS::Constant<float>( kA, kB, -kB, -kA, -kA, -kB, kB, kA ), indexGradient );
            gY = FN_PermuteTable8( FS::Constant<float>( kB, kA, kA, kB, -kB, -kA, -kA, -kB ), indexGradient );
            vX = FN_PermuteTable8( FS::Constant<float>( kA, kB, -kB, -kA, -kA, -kB, kB, kA ), indexOuterVector );
            vY = FN_PermuteTable8( FS::Constant<float>( kB, kA, kA, kB, -kB, -kA, -kA, -kB ), indexOuterVector );
        }
        else
        {
            // Index bit 0: odd base direction, bit 1: rotate by 90, bit 2: rotate by 180. Both rotations by 180 are applied as
            // sign flips of the contribution: bit 31 directly, bit 28 with a select ( clang folds an isolated
            // ( ( hash >> 21 ) << 24 ) & mask into ( hash * ( A << 3 ) ) & mask, i.e. an extra multiply )
            auto rotate = []( int32v rotate90, float32v baseX, float32v baseY, float32v& outX, float32v& outY )
            {
                outX = FS::SelectHighBit( rotate90, float32v( 0 ) - baseY, baseX );
                outY = FS::SelectHighBit( rotate90, baseX, baseY );
            };

            int32v hashTop = hash >> 21;
            int32v oddGradient = hashTop << 23;
            rotate( hashTop << 22, FS::SelectHighBit( oddGradient, float32v( kB ), float32v( kA ) ), FS::SelectHighBit( oddGradient, float32v( kA ), float32v( kB ) ), gX, gY );

            int32v oddOuter = hashTop << 26;
            rotate( hashTop << 25, FS::SelectHighBit( oddOuter, float32v( kB ), float32v( kA ) ), FS::SelectHighBit( oddOuter, float32v( kA ), float32v( kB ) ), vX, vY );

            multiplier ^= FS::Cast<float>( hash & int32v( 1 << 31 ) );
            multiplier = FS::SelectHighBit( hashTop << 24, float32v( 0 ) - multiplier, multiplier );
        }

        multiplier *= FN_FMulAdd( fY, gY, fX * gX );

        valueX = FN_FMulAdd( multiplier, vX, valueX );
        valueY = FN_FMulAdd( multiplier, vY, valueY );
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FN_WARP_HELPER ApplyGradientOuterProductVectorProductCommon( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ )
    {
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 1 );
        int32v indexGradient = FS::BitShiftRightZeroExtend( hashShifted * int32v( 12 >> 2 ), 28 ); // [0,12)
        int32v indexOuterVector = ( hashShifted * int32v( 0xAAAAAAAB ) ) & int32v( 0xC0000003 ); // [0,12) in bits 0,1,30,31 // ( -4LL << 30 ) / 3 )
        indexOuterVector |= indexOuterVector >> 28;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, 0 ) );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) );

            multiplier *= FN_FMulAdd( gZ, fZ, FN_FMulAdd( fY, gY, fX * gX ) );

            valueX = FN_FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) ), valueX );
            valueY = FN_FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, 0 ) ), valueY );
            valueZ = FN_FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) ), valueZ );
        }
        else
        {
            {
                float32v sign0 = FS::Cast<float>( indexGradient << 31 );
                float32v sign1 = FS::Cast<float>( ( indexGradient >> 1 ) << 31 );

                float32v u;
                if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
                {
                    u = FS::SelectHighBit( indexGradient << ( 31 - 3 ), fY, fX );
                }
                else
                {
                    u = FS::Select( indexGradient >= int32v( 8 ), fY, fX );
                }
                float32v v = FS::Select( indexGradient >= int32v( 4 ), fZ, fY );

                multiplier *= ( u ^ sign0 ) + ( v ^ sign1 );
            }

            {
                indexOuterVector &= int32v( 0xF );

                float32v signed0 = multiplier ^ FS::Cast<float>( indexOuterVector << 31 );
                float32v signed1 = multiplier ^ FS::Cast<float>( ( indexOuterVector >> 1 ) << 31 );

                mask32v notYZ = indexOuterVector < int32v( 8 );
                mask32v notXY = indexOuterVector >= int32v( 4 );

                valueX = FS::MaskedAdd( notYZ, valueX, signed0 );
                valueZ = FS::MaskedAdd( notXY, valueZ, signed1 );
                valueY = FS::InvMaskedAdd( notYZ & notXY, valueY, FS::Select( notXY, signed0, signed1 ) );
            }
        }
    }

    // Gradient and outer vector both from the 32 vectors ( 0, +-1, +-1, +-1 ) and permutations ( as 4D Perlin ): the
    // gradient from hash bits 27-31 ( GetGradientDotPerlin ), the outer vector from bits 22-26 with the same layout
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FN_WARP_HELPER_4D ApplyGradientOuterProductVectorProductSimplex( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW )
    {
        int32v hashGradient = hash >> 27; // bits 27-31

        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) || kLatticeBlockTbl<SIMD> ) // AArch64: TBL lookup of hash & 31
        {
            multiplier *= GetGradientDotPerlin( hashGradient, fX, fY, fZ, fW );
        }
        else
        {
            // GetGradientDotPerlin's SSE path with the signs taken as ( hashGradient << n ) & mask: its ( hash >> 1 ) << 31 form
            // would be folded into ( hash * A ) << 3 and then into an extra multiply
            int32v p = hashGradient & int32v( 3 << 3 );

            float32v a = FS::Select( p > int32v( 0 ), fX, fY );
            float32v b = FS::SelectHighBit( hashGradient << 27, fY, fZ );
            float32v c = FS::Select( p > int32v( 2 << 3 ), fZ, fW );

            float32v aSign = FS::Cast<float>( hashGradient << 31 );
            float32v bSign = FS::Cast<float>( ( hashGradient << 30 ) & int32v( 1 << 31 ) );
            float32v cSign = FS::Cast<float>( ( hashGradient << 29 ) & int32v( 1 << 31 ) );

            multiplier *= ( a ^ aSign ) + ( b ^ bSign ) + ( c ^ cSign );
        }

        int32v hashOuter = hash >> 22; // bits 22-26

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v vX, vY, vZ, vW;
            GetGradientPerlin4DAVX512( hashOuter, vX, vY, vZ, vW );

            valueX = FN_FMulAdd( multiplier, vX, valueX );
            valueY = FN_FMulAdd( multiplier, vY, valueY );
            valueZ = FN_FMulAdd( multiplier, vZ, valueZ );
            valueW = FN_FMulAdd( multiplier, vW, valueW );
        }
        else
        {
            // Bits 3/4 pick the zero component: 0 -> x, 1 -> y, 2 -> z, 3 -> w. Bits 0/1/2 are the signs of the others in order
            float32v signedA = multiplier ^ FS::Cast<float>( hashOuter << 31 );
            float32v signedB = multiplier ^ FS::Cast<float>( ( hashOuter << 30 ) & int32v( 1 << 31 ) );
            float32v signedC = multiplier ^ FS::Cast<float>( ( hashOuter << 29 ) & int32v( 1 << 31 ) );

            int32v zeroAxis = hashOuter & int32v( 3 << 3 );
            mask32v notZeroX = zeroAxis != int32v( 0 );
            mask32v zeroW = zeroAxis == int32v( 3 << 3 );
            int32v zeroZOrW = hashOuter << 27;

            valueX = FS::MaskedAdd( notZeroX, valueX, signedA );
            valueY += FS::SelectHighBit( zeroZOrW, signedB, FS::InvMasked( notZeroX, signedA ) );
            valueZ += FS::SelectHighBit( zeroZOrW, FS::Masked( zeroW, signedC ), signedB );
            valueW = FS::InvMaskedAdd( zeroW, valueW, signedC );
        }
    }

    // Rotation by a multiple of 45 degrees ( 3 hash bits into an 8 entry cos/sin table ) and a hashed reflection: the dihedral group D8
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static void ApplyOrthogonalGradientMatrixVectorProductSimplex( int32v hash, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY )
    {
        constexpr float kR = 0.7071067811865476f;
        float32v cos, sin, reflect; // Bit 31: reflection

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            int32v index = hash >> 28; // permutexvar reads 4 bits, the tables repeat after 8

            cos = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, kR, 0, -kR, -1, -kR, 0, kR, 1, kR, 0, -kR, -1, -kR, 0, kR ) );
            sin = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 0, kR, 1, kR, 0, -kR, -1, -kR, 0, kR, 1, kR, 0, -kR, -1, -kR ) );
            reflect = FS::Cast<float>( hash & int32v( 1 << 31 ) );
        }
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) || kLatticeBlockTbl<SIMD> ) // 8 entry table lookups: vpermps / AArch64 TBL
        {
            int32v index = hash >> 28; // vpermps only reads the low 3 bits
            if constexpr( !( SIMD & FastSIMD::FeatureFlag::AVX2 ) )
            {
                index &= int32v( 7 ); // AArch64 TBL reads 0 past the table
            }

            cos = FN_PermuteTable8( FS::Constant<float>( 1, kR, 0, -kR, -1, -kR, 0, kR ), index );
            sin = FN_PermuteTable8( FS::Constant<float>( 0, kR, 1, kR, 0, -kR, -1, -kR ), index );
            reflect = FS::Cast<float>( hash & int32v( 1 << 31 ) );
        }
        else
        {
            // Bit 28: diagonal, bit 29: rotate by 90, bit 30: rotate by 180
            int32v hashTop = hash >> 21;
            int32v diagonal = hashTop << 24;
            float32v baseCos = FS::SelectHighBit( diagonal, float32v( kR ), float32v( 1 ) );
            float32v baseSin = FS::SelectHighBit( diagonal, float32v( kR ), float32v( 0 ) );

            // The rotation by 180 is applied as a sign flip of the contribution with a select: clang folds an isolated
            // ( ( hash >> 21 ) << 22 ) & mask into ( hash * ( A << 1 ) ) & mask, i.e. an extra multiply
            int32v rotate90 = hashTop << 23;
            cos = FS::SelectHighBit( rotate90, float32v( 0 ) - baseSin, baseCos );
            sin = FS::SelectHighBit( rotate90, baseCos, baseSin );
            multiplier = FS::SelectHighBit( hashTop << 22, float32v( 0 ) - multiplier, multiplier );
            reflect = FS::Cast<float>( hash & int32v( 1 << 31 ) );
        }

        valueX = FN_FMulAdd( multiplier, FN_FNMulAdd( fY, sin, fX * cos ), valueX );
        multiplier ^= reflect;
        valueY = FN_FMulAdd( multiplier, FN_FMulAdd( fY, cos, fX * sin ), valueY );
    }

    // Hashed orthogonal matrices built as S * P * M: M is a hashed choice between identity and a fixed orthogonal matrix
    // with no zero entries that is cheap to apply (with one input sign optionally flipped, so its rows cover more
    // directions), P a hashed axis permutation and S hashed per axis sign flips. The outer signed permutations act
    // irreducibly, so the warp covariance is isotropic like with a general random rotation, while the inner choice
    // keeps the matrix rows from all being axis aligned. Only selects, sign flips and a few adds, no table lookups.
    // hashTop = HashPrimesHB() >> 21: bit k of hashTop is hash bit 21 + k

    template<typename F, typename I>
    FS_FORCEINLINE static F FlipSign( F value, I hashTop, int bit )
    {
        return value ^ FS::Cast<float>( ( hashTop << ( 31 - bit ) ) & I( 1 << 31 ) );
    }

    // M = ( 2, 2, -1 ) / 3 row permutations: ( 2/3 sum - z, 2/3 sum - y, 2/3 sum - x ). P: cyclic axis rotation [0,3)
    // Bits: rotation from the whole top of the hash ( signed compares ), 21 z sign, 22 mix, 23-25 output signs
    template<typename F, typename I>
    FS_FORCEINLINE static void ApplyOrthogonalMatrixVectorProductImpl( I hash, F fX, F fY, F fZ, F multiplier, F& valueX, F& valueY, F& valueZ )
    {
        I hashTop = hash >> 21;
        fZ ^= FS::Cast<float>( hashTop << 31 ); // ( 2, 2, -1 ) and ( 2, 2, 1 ) row variants

        I useMix = hashTop << 30;
        F sum = ( fX + fY + fZ ) * F( 2.0f / 3.0f );
        F mX = FS::SelectHighBit( useMix, sum - fZ, fX );
        F mY = FS::SelectHighBit( useMix, sum - fY, fY );
        F mZ = FS::SelectHighBit( useMix, sum - fX, fZ );

        // Rotation floor( 3 * ( hash + 2^31 ) / 2^32 )
        auto rotate2 = hash > I( 715827882 );
        auto rotate12 = hash > I( -715827883 );

        F pX = FS::Select( rotate2, mZ, FS::Select( rotate12, mY, mX ) );
        F pY = FS::Select( rotate2, mX, FS::Select( rotate12, mZ, mY ) );
        F pZ = FS::Select( rotate2, mY, FS::Select( rotate12, mX, mZ ) );

        valueX = FN_FMulAdd( FlipSign( multiplier, hashTop, 2 ), pX, valueX );
        valueY = FN_FMulAdd( FlipSign( multiplier, hashTop, 3 ), pY, valueY );
        valueZ = FN_FMulAdd( FlipSign( multiplier, hashTop, 4 ), pZ, valueZ );
    }

    // M = 4x4 Hadamard / 2. P: Klein four-group axis permutation ( pair swaps )
    // Bits: 21 w sign, 22-25 output signs, 29 mix, 30 swap pairs, 31 swap adjacent
    template<typename F, typename I>
    FS_FORCEINLINE static void ApplyOrthogonalMatrixVectorProductImpl( I hash, F fX, F fY, F fZ, F fW, F multiplier, F& valueX, F& valueY, F& valueZ, F& valueW )
    {
        I hashTop = hash >> 21;
        fW ^= FS::Cast<float>( hashTop << 31 ); // Hadamard rows with an even or odd number of negative entries

        I useMix = hashTop << 23;
        F sumXY = fX + fY, diffXY = fX - fY;
        F sumZW = fZ + fW, diffZW = fZ - fW;
        F mX = FS::SelectHighBit( useMix, sumXY + sumZW, fX );
        F mY = FS::SelectHighBit( useMix, diffXY + diffZW, fY );
        F mZ = FS::SelectHighBit( useMix, sumXY - sumZW, fZ );
        F mW = FS::SelectHighBit( useMix, diffXY - diffZW, fW );
        multiplier *= FS::SelectHighBit( useMix, F( 0.5f ), F( 1.0f ) );

        F aX = FS::SelectHighBit( hash, mY, mX );
        F aY = FS::SelectHighBit( hash, mX, mY );
        F aZ = FS::SelectHighBit( hash, mW, mZ );
        F aW = FS::SelectHighBit( hash, mZ, mW );

        I swapPairs = hashTop << 22;
        F pX = FS::SelectHighBit( swapPairs, aZ, aX );
        F pY = FS::SelectHighBit( swapPairs, aW, aY );
        F pZ = FS::SelectHighBit( swapPairs, aX, aZ );
        F pW = FS::SelectHighBit( swapPairs, aY, aW );

        valueX = FN_FMulAdd( FlipSign( multiplier, hashTop, 1 ), pX, valueX );
        valueY = FN_FMulAdd( FlipSign( multiplier, hashTop, 2 ), pY, valueY );
        valueZ = FN_FMulAdd( FlipSign( multiplier, hashTop, 3 ), pZ, valueZ );
        valueW = FN_FMulAdd( FlipSign( multiplier, hashTop, 4 ), pW, valueW );
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static void ApplyOrthogonalMatrixVectorProduct( int32v hash, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ )
    {
        ApplyOrthogonalMatrixVectorProductImpl( hash, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
    }

    // 4D below AVX512: process each native register half separately, skipping halves with no contribution
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FN_WARP_HELPER_4D ApplyOrthogonalMatrixVectorProduct( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            ApplyOrthogonalMatrixVectorProductImpl( hash, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
        }
        else
        {
            if( FS::AnyMask( multiplier.v0 != float32v::DoubledType( 0 ) ) )
            {
                ApplyOrthogonalMatrixVectorProductImpl( hash.v0, fX.v0, fY.v0, fZ.v0, fW.v0, multiplier.v0, valueX.v0, valueY.v0, valueZ.v0, valueW.v0 );
            }
            if( FS::AnyMask( multiplier.v1 != float32v::DoubledType( 0 ) ) )
            {
                ApplyOrthogonalMatrixVectorProductImpl( hash.v1, fX.v1, fY.v1, fZ.v1, fW.v1, multiplier.v1, valueX.v1, valueY.v1, valueZ.v1, valueW.v1 );
            }
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionSimplex( int32v hash, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalGradientMatrixVectorProductSimplex( hash, fX, fY, multiplier, valueX, valueY );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductSimplex( hash, fX, fY, multiplier, valueX, valueY );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionCommon( int32v hash, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalMatrixVectorProduct( hash, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductCommon( hash, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionSimplex( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalMatrixVectorProduct( hash, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductSimplex( hash, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
        }
    }
}

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpSimplex, SIMD> final : public virtual DomainWarpSimplex, public DispatchClass<DomainWarp, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> );

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        return this->template WarpGenBlock<DispatchClass, DIMS>( seed, pos, out, count );
    }

public:
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_2D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, xOut, yOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_2D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, xOut, yOut );
        }
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme ) 
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_3D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_3D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        }        
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_4D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_4D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
    }

protected:
    template<VectorizationScheme Scheme>
    FS_FORCEINLINE float32v Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        constexpr double kRoot3 = 1.7320508075688772935274463415059;
        constexpr double kSkew2 = 1.0 / ( kRoot3 + 1.0 );
        constexpr double kUnskew2 = -1.0 / ( kRoot3 + 3.0 );
        constexpr double kFalloffRadiusSquared = 0.5;

        float32v skewDelta = float32v( kSkew2 ) * ( x + y );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );

        mask32v xGreaterEqualY = dxSkewed >= dySkewed;

        float32v unskewDelta = float32v( kUnskew2 ) * ( dxSkewed + dySkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;

        float32v dx1 = FS::MaskedIncrement( ~xGreaterEqualY, dx0 ) - float32v( kUnskew2 + 1 );
        float32v dy1 = FS::MaskedIncrement( xGreaterEqualY, dy0 ) - float32v( kUnskew2 + 1 );
        float32v dx2 = dx0 - float32v( kUnskew2 * 2 + 1 );
        float32v dy2 = dy0 - float32v( kUnskew2 * 2 + 1 );

        float32v falloff0 = FN_FNMulAdd( dx0, dx0, FN_FNMulAdd( dy0, dy0, float32v( kFalloffRadiusSquared ) ) );
        float32v falloff1 = FN_FNMulAdd( dx1, dx1, FN_FNMulAdd( dy1, dy1, float32v( kFalloffRadiusSquared ) ) );
        float32v falloff2 = falloff0 + FN_FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot3 + 2.0 ) / ( kRoot3 + 3.0 ) ),
            float32v( -2.0 / 3.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;

        float32v valueX( 0 );
        float32v valueY( 0 );

        ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 2>( seed, xPrimedBase, yPrimedBase ), dx0, dy0, falloff0, valueX, valueY );
        ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 2>( seed, FS::MaskedAdd( xGreaterEqualY, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( xGreaterEqualY, yPrimedBase, int32v( Primes::Y ) ) ), dx1, dy1, falloff1, valueX, valueY );
        ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 2>( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) ), dx2, dy2, falloff2, valueX, valueY );

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            100.040101f :
            99.2043346f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );

        return FN_FMulAdd( valueY, valueY, valueX * valueX );
    }

    template<VectorizationScheme Scheme>
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v skewDelta = float32v( kSkew3 ) * ( x + y + z );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );

        mask32v xGreaterEqualY = dxSkewed >= dySkewed;
        mask32v yGreaterEqualZ = dySkewed >= dzSkewed;
        mask32v xGreaterEqualZ = dxSkewed >= dzSkewed;

        float32v unskewDelta = float32v( kReflectUnskew3 ) * ( dxSkewed + dySkewed + dzSkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;
        float32v dz0 = dzSkewed + unskewDelta;

        mask32v maskX1 = xGreaterEqualY & xGreaterEqualZ;
        mask32v maskY1 = FS::BitwiseAndNot( yGreaterEqualZ, xGreaterEqualY );
        mask32v maskZ1 = xGreaterEqualZ | yGreaterEqualZ; // Inv masked

        mask32v nMaskX2 = xGreaterEqualY | xGreaterEqualZ; // Inv masked
        mask32v nMaskY2 = FS::BitwiseAndNot( xGreaterEqualY, yGreaterEqualZ );
        mask32v nMaskZ2 = xGreaterEqualZ & yGreaterEqualZ;

        // AVX512 orthogonal matrix scheme: masked compares build each vertex mask in one instruction, a and( not ) of two compares
        // is a compare under the other mask (the k logic above is then dead code). Fewer live k masks, it spilled them. The other
        // scheme does not spill and the extra compares compete with its FMAs
        if constexpr( kUseMaskedGreater && ( SIMD & FastSIMD::FeatureFlag::AVX512_F ) && Scheme == VectorizationScheme::OrthogonalGradientMatrix )
        {
            maskX1 = MaskedGreater<true>( xGreaterEqualY, dxSkewed, dzSkewed );
            maskY1 = MaskedGreater<false>( yGreaterEqualZ, dySkewed, dxSkewed );
            nMaskY2 = MaskedGreater<false>( xGreaterEqualY, dzSkewed, dySkewed );
            nMaskZ2 = MaskedGreater<true>( xGreaterEqualZ, dySkewed, dzSkewed );
        }

        float32v dx3 = dx0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dy3 = dy0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dz3 = dz0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dx1 = FS::MaskedSub( maskX1, dx3, float32v( 1 ) ); // kReflectUnskew3 * 3 + 1 = kReflectUnskew3, so dx0 - kReflectUnskew3 = dx3
        float32v dy1 = FS::MaskedSub( maskY1, dy3, float32v( 1 ) );
        float32v dz1 = FS::InvMaskedSub( maskZ1, dz3, float32v( 1 ) );
        float32v dx2 = FS::MaskedIncrement( ~nMaskX2, dx0 ); // kReflectUnskew3 * 2 - 1 = 0, so dx0 + ( kReflectUnskew3 * 2 - 1 ) = dx0
        float32v dy2 = FS::MaskedIncrement( nMaskY2, dy0 );
        float32v dz2 = FS::MaskedIncrement( nMaskZ2, dz0 );

        float32v falloff0 = FN_FNMulAdd( dz0, dz0, FN_FNMulAdd( dy0, dy0, FN_FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff1 = FN_FNMulAdd( dz1, dz1, FN_FNMulAdd( dy1, dy1, FN_FNMulAdd( dx1, dx1, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff2 = FN_FNMulAdd( dz2, dz2, FN_FNMulAdd( dy2, dy2, FN_FNMulAdd( dx2, dx2, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff3 = falloff0 - ( unskewDelta + float32v( 3.0 / 4.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );
        falloff3 = FS::Max( falloff3, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;
        falloff3 *= falloff3; falloff3 *= falloff3;

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );

        ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dx0, dy0, dz0, falloff0, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( seed, FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ), FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ) ), dx1, dy1, dz1, falloff1, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( seed, FS::MaskedAdd( nMaskX2, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( nMaskY2, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( nMaskZ2, zPrimedBase, int32v( Primes::Z ) ) ), dx2, dy2, dz2, falloff2, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ) ), dx3, dy3, dz3, falloff3, valueX, valueY, valueZ );

        if constexpr( Scheme != VectorizationScheme::OrthogonalGradientMatrix )
        {
            // Match gradient orientation.
            constexpr double kReflect3D = -2.0 / 3.0; // Reflection through the plane normal to ( 1, 1, 1 )
            float32v valueTransformDelta = float32v( kReflect3D ) * ( valueX + valueY + valueZ );
            valueX += valueTransformDelta;
            valueY += valueTransformDelta;
            valueZ += valueTransformDelta;
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(32.69428253173828125 / 1.4142135623730951) :
            39.8816903f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );
        zOut = FN_FMulAdd( valueZ, warpAmp, zOut );

        return FN_FMulAdd( valueZ, valueZ, FN_FMulAdd( valueY, valueY, valueX * valueX ) );
    }

    template<VectorizationScheme Scheme>
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v skewDelta = float32v( kSkew4 ) * ( x + y + z + w );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v wSkewed = w + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v wSkewedBase = FS::Floor( wSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;
        float32v dwSkewed = wSkewed - wSkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );
        int32v wPrimedBase = FS::Convert<int32_t>( wSkewedBase ) * int32v( Primes::W );

        float32v unskewDelta = float32v( kUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;
        float32v dz0 = dzSkewed + unskewDelta;
        float32v dw0 = dwSkewed + unskewDelta;

        int32v rankX( 0 );
        int32v rankY( 0 );
        int32v rankZ( 0 );
        int32v rankW( 0 );

        mask32v xGreaterEqualY = dx0 >= dy0;
        rankX = FS::MaskedIncrement( xGreaterEqualY, rankX );
        rankY = FS::MaskedIncrement( ~xGreaterEqualY, rankY );

        mask32v xGreaterEqualZ = dx0 >= dz0;
        rankX = FS::MaskedIncrement( xGreaterEqualZ, rankX );
        rankZ = FS::MaskedIncrement( ~xGreaterEqualZ, rankZ );

        mask32v xGreaterEqualW = dx0 >= dw0;
        rankX = FS::MaskedIncrement( xGreaterEqualW, rankX );
        rankW = FS::MaskedIncrement( ~xGreaterEqualW, rankW );

        mask32v yGreaterEqualZ = dy0 >= dz0;
        rankY = FS::MaskedIncrement( yGreaterEqualZ, rankY );
        rankZ = FS::MaskedIncrement( ~yGreaterEqualZ, rankZ );

        mask32v yGreaterEqualW = dy0 >= dw0;
        rankY = FS::MaskedIncrement( yGreaterEqualW, rankY );
        rankW = FS::MaskedIncrement( ~yGreaterEqualW, rankW );

        mask32v zGreaterEqualW = dz0 >= dw0;
        rankZ = FS::MaskedIncrement( zGreaterEqualW, rankZ );
        rankW = FS::MaskedIncrement( ~zGreaterEqualW, rankW );

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v valueW( 0 );

        // Each vertex is computed right before its contribution so fewer values stay live across the contribution calls
        float32v falloff0 = FN_FNMulAdd( dw0, dw0, FN_FNMulAdd( dz0, dz0, FN_FNMulAdd( dy0, dy0, FN_FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff4 = falloff0 + FN_FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot5 + 3.0 ) / ( kRoot5 + 5.0 ) ),
            float32v( -4.0 / 5.0 ) );
        {
            float32v falloff = FS::Max( falloff0, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0, falloff, valueX, valueY, valueZ, valueW );
        }

        // Vertex i (1 to 3) steps along the axes ranked above 3 - i
        auto applyRankedVertex = [&]( int rankThreshold, float unskewOffset )
        {
            mask32v maskX = rankX > int32v( rankThreshold );
            mask32v maskY = rankY > int32v( rankThreshold );
            mask32v maskZ = rankZ > int32v( rankThreshold );
            mask32v maskW = rankW > int32v( rankThreshold );

            float32v dx = FS::MaskedSub( maskX, dx0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dy = FS::MaskedSub( maskY, dy0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dz = FS::MaskedSub( maskZ, dz0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dw = FS::MaskedSub( maskW, dw0, float32v( 1 ) ) - float32v( unskewOffset );

            float32v falloff = FN_FNMulAdd( dw, dw, FN_FNMulAdd( dz, dz, FN_FNMulAdd( dy, dy, FN_FNMulAdd( dx, dx, float32v( kFalloffRadiusSquared ) ) ) ) );
            falloff = FS::Max( falloff, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( seed,
                FS::MaskedAdd( maskX, xPrimedBase, int32v( Primes::X ) ),
                FS::MaskedAdd( maskY, yPrimedBase, int32v( Primes::Y ) ),
                FS::MaskedAdd( maskZ, zPrimedBase, int32v( Primes::Z ) ),
                FS::MaskedAdd( maskW, wPrimedBase, int32v( Primes::W ) ) ), dx, dy, dz, dw, falloff, valueX, valueY, valueZ, valueW );
        };

        applyRankedVertex( 2, float( kUnskew4 ) );
        applyRankedVertex( 1, float( kUnskew4 * 2 ) );
        applyRankedVertex( 0, float( kUnskew4 * 3 ) );

        {
            float32v falloff = FS::Max( falloff4, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( seed,
                xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ), wPrimedBase + int32v( Primes::W ) ),
                dx0 - float32v( kUnskew4 * 4 + 1 ), dy0 - float32v( kUnskew4 * 4 + 1 ), dz0 - float32v( kUnskew4 * 4 + 1 ), dw0 - float32v( kUnskew4 * 4 + 1 ),
                falloff, valueX, valueY, valueZ, valueW );
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            15.7187556f :
            43.6732089f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        valueW *= float32v( kBounding * this->mAxisScale[3] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );
        zOut = FN_FMulAdd( valueZ, warpAmp, zOut );
        wOut = FN_FMulAdd( valueW, warpAmp, wOut );

        return FN_FMulAdd( valueW, valueW, FN_FMulAdd( valueZ, valueZ, FN_FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpSuperSimplex, SIMD> final : public virtual DomainWarpSuperSimplex, public DispatchClass<DomainWarp, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> );

public:
    static constexpr bool kHasGenBlock = true;

    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        return this->template WarpGenBlock<DispatchClass, DIMS>( seed, pos, out, count );
    }

public:
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_2D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, xOut, yOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_2D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, xOut, yOut );
        }
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme ) 
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_3D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_3D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        }        
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_4D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_4D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
    }

protected:
    // Vertex direction is chosen by the sign bit of signValue, blendv only reads the high bit so no compare is needed
    template<typename T>
    FS_FORCEINLINE static T SelectSign( float32v signValue, T ifNegative, T ifPositive )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Select( signValue < float32v( 0 ), ifNegative, ifPositive );
        }
        else
        {
            return FS::SelectHighBit( signValue, ifNegative, ifPositive );
        }
    }

    FS_FORCEINLINE static float32v SignBit( float32v signValue )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Masked( signValue < float32v( 0 ), float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
        }
        else
        {
            return signValue & FS::Cast<float>( int32v( 1 << 31 ) );
        }
    }

    // True if any lane of the clamped falloff is in range. Out of range lanes are exactly +0, so on SSE4.1/AVX2 OR the
    // halves and test for any set bit instead of compare + movemask per half
    template<typename F>
    FS_FORCEINLINE static bool AnyInRange( F falloffBase )
    {
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            auto bits = _mm256_castps_si256( _mm256_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm256_testz_si256( bits, bits );
        }
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::SSE41 ) && !( SIMD & FastSIMD::FeatureFlag::AVX ) )
        {
            auto bits = _mm_castps_si128( _mm_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm_testz_si128( bits, bits );
        }
        else
        {
            return FS::AnyMask( falloffBase != F( 0 ) );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE float32v Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        constexpr double kRoot3 = 1.7320508075688772935274463415059;
        constexpr double kSkew2 = 1.0 / ( kRoot3 + 1.0 );
        constexpr double kUnskew2 = -1.0 / ( kRoot3 + 3.0 );
        constexpr double kFalloffRadiusSquared = 2.0 / 3.0;

        float32v skewDelta = float32v( kSkew2 ) * ( x + y );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );

        mask32v forwardXY = dxSkewed + dySkewed > float32v( 1.0f );
        float32v boundaryXY = FS::Masked( forwardXY, float32v( -1.0f ) );
        mask32v forwardX = FN_FMulAdd( dxSkewed, float32v( -2.0f ), dySkewed ) < boundaryXY;
        mask32v forwardY = FN_FMulAdd( dySkewed, float32v( -2.0f ), dxSkewed ) < boundaryXY;

        float32v unskewDelta = float32v( kUnskew2 ) * ( dxSkewed + dySkewed );
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;

        float32v falloffBase0;
        float32v valueX( 0 );
        float32v valueY( 0 );

        // Vertex <0, 0>
        {
            int32v hash = HashWarpVertex<Scheme, 2>( seed, xPrimedBase, yPrimedBase );
            falloffBase0 = FN_FNMulAdd( dxBase, dxBase, FN_FNMulAdd( dyBase, dyBase, float32v( kFalloffRadiusSquared ) ) );
            float32v falloff = falloffBase0; falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dxBase, dyBase, falloff, valueX, valueY );
        }

        // Vertex <1, 1>
        {
            int32v hash = HashWarpVertex<Scheme, 2>( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) );
            float32v falloff = FN_FMulAdd( unskewDelta,
                float32v( -4.0 * ( kRoot3 + 2.0 ) / ( kRoot3 + 3.0 ) ),
                falloffBase0 - float32v( kFalloffRadiusSquared ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dxBase - float32v( 2 * kUnskew2 + 1 ), dyBase - float32v( 2 * kUnskew2 + 1 ), falloff, valueX, valueY );
        }

        float32v xyDelta = FS::Select( forwardXY, float32v( kUnskew2 + 1 ), float32v( -kUnskew2 ) );
        dxBase -= xyDelta;
        dyBase -= xyDelta;

        // Vertex <1, 0> or <-1, 0> or <1, 2>
        {
            int32v hash = HashWarpVertex<Scheme, 2>( seed,
                FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardX, xPrimedBase, int32v( Primes::X * 2 ) ), int32v( Primes::X ) ),
                FS::MaskedAdd( forwardXY, yPrimedBase, int32v( Primes::Y ) ) );
            float32v dx = dxBase - FS::Select( forwardX, float32v( 1 + 2 * kUnskew2 ), float32v( -1 ) );
            float32v dy = FS::MaskedSub( forwardX, dyBase, float32v( 2 * kUnskew2 ) );
            float32v falloff = FS::Max( FN_FNMulAdd( dx, dx, FN_FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dx, dy, falloff, valueX, valueY );
        }

        // Vertex <0, 1> or <0, -1> or <2, 1>
        {
            int32v hash = HashWarpVertex<Scheme, 2>( seed,
                FS::MaskedAdd( forwardXY, xPrimedBase, int32v( Primes::X ) ),
                FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardY, yPrimedBase, int32v( (int32_t)( Primes::Y * 2LL ) ) ), int32v( Primes::Y ) ) );
            float32v dx = FS::MaskedSub( forwardY, dxBase, float32v( 2 * kUnskew2 ) );
            float32v dy = dyBase - FS::Select( forwardY, float32v( 1 + 2 * kUnskew2 ), float32v( -1 ) );
            float32v falloff = FS::Max( FN_FNMulAdd( dx, dx, FN_FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dx, dy, falloff, valueX, valueY );
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            18.5526678f :
            18.2335248f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );

        return FN_FMulAdd( valueY, valueY, valueX * valueX );
    }

    template<VectorizationScheme Scheme>
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kTwiceUnskew3 = -1.0 / 4.0;

        constexpr double kDistanceSquaredA = 3.0 / 4.0;
        constexpr double kDistanceSquaredB = 1.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        float32v skewDelta = float32v( kSkew3 ) * ( x + y + z );

        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;

        // From unit cell base, find closest vertex
        {
            // Perform a double unskew to get the vector whose dot product with skewed vectors produces the unskewed result.
            float32v twiceUnskewDelta = float32v( kTwiceUnskew3 ) * ( dxSkewed + dySkewed + dzSkewed );
            float32v xNormal = dxSkewed + twiceUnskewDelta;
            float32v yNormal = dySkewed + twiceUnskewDelta;
            float32v zNormal = dzSkewed + twiceUnskewDelta;
            float32v xyzNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal

            // Using those, compare scores to determine which vertex is closest.
            constexpr auto considerVertex = [] ( float32v& maxScore, int32v& moveMaskBits, float32v score, int32v bits ) constexpr
                {
                    moveMaskBits = FS::Select( score > maxScore, bits, moveMaskBits );
                    maxScore = FS::Max( maxScore, score );
                };
            float32v maxScore = float32v( 0.375f );
            int32v moveMaskBits = FS::Masked( xyzNormal > maxScore, int32v( -1 ) );
            maxScore = FS::Max( maxScore, xyzNormal );
            considerVertex( maxScore, moveMaskBits, xNormal, 0b001 );
            considerVertex( maxScore, moveMaskBits, yNormal, 0b010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b100 );
            maxScore += float32v( 0.125f ) - xyzNormal;
            considerVertex( maxScore, moveMaskBits, -zNormal, 0b011 );
            considerVertex( maxScore, moveMaskBits, -yNormal, 0b101 );
            considerVertex( maxScore, moveMaskBits, -xNormal, 0b110 );

            mask32v moveX = ( moveMaskBits & int32v( 0b001 ) ) != int32v( 0 );
            mask32v moveY = ( moveMaskBits & int32v( 0b010 ) ) != int32v( 0 );
            mask32v moveZ = ( moveMaskBits & int32v( 0b100 ) ) != int32v( 0 );

            xSkewedBase = FS::MaskedIncrement( moveX, xSkewedBase );
            ySkewedBase = FS::MaskedIncrement( moveY, ySkewedBase );
            zSkewedBase = FS::MaskedIncrement( moveZ, zSkewedBase );

            dxSkewed = FS::MaskedDecrement( moveX, dxSkewed );
            dySkewed = FS::MaskedDecrement( moveY, dySkewed );
            dzSkewed = FS::MaskedDecrement( moveZ, dzSkewed );
        }

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis direction
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase;
        int32v xPrimedDeltaPos = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v xPrimedDeltaNeg = xPrimedBase ^ ( xPrimedBase - int32v( Primes::X ) );
        int32v yPrimedDeltaPos = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v yPrimedDeltaNeg = yPrimedBase ^ ( yPrimedBase - int32v( Primes::Y ) );
        int32v zPrimedDeltaPos = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v zPrimedDeltaNeg = zPrimedBase ^ ( zPrimedBase - int32v( Primes::Z ) );

        float32v skewedCoordinateSum = dxSkewed + dySkewed + dzSkewed;
        float32v twiceUnskewDelta = float32v( kTwiceUnskew3 ) * skewedCoordinateSum;
        float32v xNormal = dxSkewed + twiceUnskewDelta;
        float32v yNormal = dySkewed + twiceUnskewDelta;
        float32v zNormal = dzSkewed + twiceUnskewDelta;
        float32v xyzNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal

        float32v unskewDelta = float32v( kReflectUnskew3 ) * skewedCoordinateSum;
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;
        float32v dzBase = dzSkewed + unskewDelta;

        float32v coordinateSum = float32v( 1 + 3 * kReflectUnskew3 ) * skewedCoordinateSum; // dxBase + dyBase + dzBase

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v falloffBaseStemA, falloffBaseStemB;

        // Vertex <0, 0, 0>
        {
            float32v falloffBase = FN_FNMulAdd( dzBase, dzBase, FN_FNMulAdd( dyBase, dyBase, FN_FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dxBase, dyBase, dzBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <1, 1, 1> or <-1, -1, -1>
        {
            float32v signValue = xyzNormal;

            float32v sign = SignBit( signValue );
            float32v offset = float32v( 3 * kReflectUnskew3 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, xDelta, yDelta, zDelta ), dxBase - offset, dyBase - offset, dzBase - offset,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 1, 0> or <-1, -1, 0>
        {
            float32v signValue = xyzNormal - zNormal; // Sign bit set when xyzNormal < zNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dzBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, xDelta, yDelta ), dxBase, dyBase, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 0, 1> or <-1, 0, -1>
        {
            float32v signValue = xyzNormal - yNormal; // Sign bit set when xyzNormal < yNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dyBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, xDelta, zDelta ), dxBase, dyBase - offset0, dzBase,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 1, 1> or <0, -1, -1>
        {
            float32v signValue = xyzNormal - xNormal; // Sign bit set when xyzNormal < xNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dxBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase, dzBase,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 0, 0> or <-1, 0, 0>
        {
            float32v signValue = xNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, xDelta ), dxBase + offset0, dyBase - offset0, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 1, 0> or <0, -1, 0>
        {
            float32v signValue = yNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, yDelta ), dxBase - offset0, dyBase + offset0, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 0, 1> or <0, 0, -1>
        {
            float32v signValue = zNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashWarpVertex<Scheme, 3>( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase + offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        if constexpr( Scheme != VectorizationScheme::OrthogonalGradientMatrix )
        {
            // Match gradient orientation.
            constexpr double kReflect3D = -2.0 / 3.0;
            float32v valueTransformDelta = float32v( kReflect3D ) * ( valueX + valueY + valueZ );
            valueX += valueTransformDelta;
            valueY += valueTransformDelta;
            valueZ += valueTransformDelta;
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(144.736422163332608 / 1.4142135623730951) :
            184.384454f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );
        zOut = FN_FMulAdd( valueZ, warpAmp, zOut );

        return FN_FMulAdd( valueZ, valueZ, FN_FMulAdd( valueY, valueY, valueX * valueX ) );
    }

    template<VectorizationScheme Scheme>
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kTwiceUnskew4 = -1.0 / 5.0;

        constexpr double kDistanceSquaredA = 4.0 / 5.0;
        constexpr double kDistanceSquaredB = 6.0 / 5.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        float32v skewDelta = float32v( kSkew4 ) * ( x + y + z + w );

        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v wSkewed = w + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v wSkewedBase = FS::Floor( wSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;
        float32v dwSkewed = wSkewed - wSkewedBase;

        // From unit cell base, find closest vertex
        {
            // Perform a double unskew to get the vector whose dot product with skewed vectors produces the unskewed result.
            float32v twiceUnskewDelta = float32v( kTwiceUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
            float32v xNormal = dxSkewed + twiceUnskewDelta;
            float32v yNormal = dySkewed + twiceUnskewDelta;
            float32v zNormal = dzSkewed + twiceUnskewDelta;
            float32v wNormal = dwSkewed + twiceUnskewDelta;
            float32v xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

            // Using those, compare scores to determine which vertex is closest.
            constexpr auto considerVertex = [] ( float32v& maxScore, int32v& moveMaskBits, float32v score, int32v bits ) constexpr
                {
                    moveMaskBits = FS::Select( score > maxScore, bits, moveMaskBits );
                    maxScore = FS::Max( maxScore, score );
                };
            float32v maxScore = float32v( 0.6f ) - xyzwNormal;
            int32v moveMaskBits = FS::Masked( float32v( 0.2f ) > maxScore, int32v( -1 ) );
            maxScore = FS::Max( maxScore, float32v( 0.2f ) );
            considerVertex( maxScore, moveMaskBits, -wNormal, 0b0111 );
            considerVertex( maxScore, moveMaskBits, -zNormal, 0b1011 );
            considerVertex( maxScore, moveMaskBits, -yNormal, 0b1101 );
            considerVertex( maxScore, moveMaskBits, -xNormal, 0b1110 );
            maxScore += xyzwNormal - float32v( 0.2f );
            considerVertex( maxScore, moveMaskBits, xNormal, 0b0001 );
            considerVertex( maxScore, moveMaskBits, yNormal, 0b0010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b0100 );
            considerVertex( maxScore, moveMaskBits, wNormal, 0b1000 );
            maxScore += float32v( 0.2f ) - xNormal;
            considerVertex( maxScore, moveMaskBits, yNormal, 0b0011 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b0101 );
            considerVertex( maxScore, moveMaskBits, wNormal, 0b1001 );
            maxScore += xNormal;
            considerVertex( maxScore, moveMaskBits, yNormal + zNormal, 0b0110 );
            maxScore -= wNormal;
            considerVertex( maxScore, moveMaskBits, yNormal, 0b1010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b1100 );

            mask32v moveX = ( moveMaskBits & int32v( 0b0001 ) ) != int32v( 0 );
            mask32v moveY = ( moveMaskBits & int32v( 0b0010 ) ) != int32v( 0 );
            mask32v moveZ = ( moveMaskBits & int32v( 0b0100 ) ) != int32v( 0 );
            mask32v moveW = ( moveMaskBits & int32v( 0b1000 ) ) != int32v( 0 );

            xSkewedBase = FS::MaskedIncrement( moveX, xSkewedBase );
            ySkewedBase = FS::MaskedIncrement( moveY, ySkewedBase );
            zSkewedBase = FS::MaskedIncrement( moveZ, zSkewedBase );
            wSkewedBase = FS::MaskedIncrement( moveW, wSkewedBase );

            dxSkewed = FS::MaskedDecrement( moveX, dxSkewed );
            dySkewed = FS::MaskedDecrement( moveY, dySkewed );
            dzSkewed = FS::MaskedDecrement( moveZ, dzSkewed );
            dwSkewed = FS::MaskedDecrement( moveW, dwSkewed );
        }

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );
        int32v wPrimedBase = FS::Convert<int32_t>( wSkewedBase ) * int32v( Primes::W );

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis direction
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase ^ wPrimedBase;
        int32v xPrimedDeltaPos = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v xPrimedDeltaNeg = xPrimedBase ^ ( xPrimedBase - int32v( Primes::X ) );
        int32v yPrimedDeltaPos = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v yPrimedDeltaNeg = yPrimedBase ^ ( yPrimedBase - int32v( Primes::Y ) );
        int32v zPrimedDeltaPos = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v zPrimedDeltaNeg = zPrimedBase ^ ( zPrimedBase - int32v( Primes::Z ) );
        int32v wPrimedDeltaPos = wPrimedBase ^ ( wPrimedBase + int32v( Primes::W ) );
        int32v wPrimedDeltaNeg = wPrimedBase ^ ( wPrimedBase - int32v( Primes::W ) );

        float32v skewedCoordinateSum = dxSkewed + dySkewed + dzSkewed + dwSkewed;
        float32v twiceUnskewDelta = float32v( kTwiceUnskew4 ) * skewedCoordinateSum;
        float32v xNormal = dxSkewed + twiceUnskewDelta;
        float32v yNormal = dySkewed + twiceUnskewDelta;
        float32v zNormal = dzSkewed + twiceUnskewDelta;
        float32v wNormal = dwSkewed + twiceUnskewDelta;
        float32v xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

        float32v unskewDelta = float32v( kUnskew4 ) * skewedCoordinateSum;
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;
        float32v dzBase = dzSkewed + unskewDelta;
        float32v dwBase = dwSkewed + unskewDelta;

        float32v coordinateSum = float32v( 1 + 4 * kUnskew4 ) * skewedCoordinateSum; // dxBase + dyBase + dzBase + dwBase

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v valueW( 0 );
        float32v falloffBaseStemA, falloffBaseStemB;

        // Vertex <0, 0, 0, 0>
        {
            float32v falloffBase = FN_FNMulAdd( dwBase, dwBase, FN_FNMulAdd( dzBase, dzBase, FN_FNMulAdd( dyBase, dyBase, FN_FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase, dyBase, dzBase, dwBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 1, 1, 1> or <-1, -1, -1, -1>
        {
            float32v signValue = xyzwNormal;
            float32v sign = SignBit( signValue );

            float32v offset = float32v( 4 * kUnskew4 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, yDelta, zDelta, wDelta ), dxBase - offset, dyBase - offset, dzBase - offset, dwBase - offset,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 1, 0> or <-1, -1, -1, 0>
        {
            float32v signValue = xyzwNormal - wNormal; // Sign bit set when xyzwNormal < wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dwBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, yDelta, zDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 0, 1> or <-1, -1, 0, -1>
        {
            float32v signValue = xyzwNormal - zNormal; // Sign bit set when xyzwNormal < zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, yDelta, wDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 1, 1> or <-1, 0, -1, -1>
        {
            float32v signValue = xyzwNormal - yNormal; // Sign bit set when xyzwNormal < yNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, zDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 1, 1> or <0, -1, -1, -1>
        {
            float32v signValue = xyzwNormal - xNormal; // Sign bit set when xyzwNormal < xNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, yDelta, zDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 0, 0> or <-1, 0, 0, 0>
        {
            float32v signValue = xNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 0, 0> or <-1, -1, 0, 0>
        {
            float32v signValue = xNormal - -yNormal; // Sign bit set when xNormal < -yNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dyBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, yDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 1, 0> or <-1, 0, -1, 0>
        {
            float32v signValue = xNormal - -zNormal; // Sign bit set when xNormal < -zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dzBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, zDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 0, 1> or <-1, 0, 0, -1>
        {
            float32v signValue = xNormal - -wNormal; // Sign bit set when xNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, xDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 0, 0> or <0, -1, 0, 0>
        {
            float32v signValue = yNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, yDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 1, 0> or <0, -1, -1, 0>
        {
            float32v signValue = yNormal - -zNormal; // Sign bit set when yNormal < -zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dzBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 0, 1> or <0, -1, 0, -1>
        {
            float32v signValue = yNormal - -wNormal; // Sign bit set when yNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, yDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 1, 0> or <0, 0, -1, 0>
        {
            float32v signValue = zNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 1, 1> or <0, 0, -1, -1>
        {
            float32v signValue = zNormal - -wNormal; // Sign bit set when zNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dzBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, zDelta, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 0, 1> or <0, 0, 0, -1>
        {
            float32v signValue = wNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FN_FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dwBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashWarpVertex<Scheme, 4>( hashBase, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            51.0465794f :
            138.028888f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        valueW *= float32v( kBounding * this->mAxisScale[3] );
        xOut = FN_FMulAdd( valueX, warpAmp, xOut );
        yOut = FN_FMulAdd( valueY, warpAmp, yOut );
        zOut = FN_FMulAdd( valueZ, warpAmp, zOut );
        wOut = FN_FMulAdd( valueW, warpAmp, wOut );

        return FN_FMulAdd( valueW, valueW, FN_FMulAdd( valueZ, valueZ, FN_FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};
