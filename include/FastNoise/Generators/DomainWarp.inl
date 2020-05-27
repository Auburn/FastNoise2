#include "FastSIMD/InlInclude.h"

#include "DomainWarp.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::DomainWarp, FS> : public virtual FastNoise::DomainWarp, public FS_T<FastNoise::Generator, FS>
{
public:
    float GetWarpFrequency() const { return mWarpFrequency; }
    const HybridSource& GetWarpAmplitude() const { return mWarpAmplitude; }

    virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const = 0;
    virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const = 0;

    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        Warp( seed, this->GetSourceValue( mWarpAmplitude, seed, pos... ), (pos * float32v( mWarpFrequency ))..., pos... );

        return this->GetSourceValue( mSource, seed, pos...);
    }
};

template<typename FS>
class FS_T<FastNoise::DomainWarpGradient, FS> : public virtual FastNoise::DomainWarpGradient, public FS_T<FastNoise::DomainWarp, FS>
{
public:
    void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        xs = InterpQuintic( x - xs );
        ys = InterpQuintic( y - ys );

    #define GRADIENT_COORD( _x, _y )\
        int32v hash##_x##_y = HashPrimesHB(seed, x##_x, y##_y );\
        float32v x##_x##_y = FS_Converti32_f32( hash##_x##_y & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );\
        float32v y##_x##_y = FS_Converti32_f32( (hash##_x##_y >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

        GRADIENT_COORD( 0, 0 );
        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        float32v normalise = warpAmp * float32v( 1.0f / (0xffff / 2.0f) );

        xOut = FS_FMulAdd_f32( Lerp( Lerp( x00, x10, xs ), Lerp( x01, x11, xs ), ys ), normalise, xOut );
        yOut = FS_FMulAdd_f32( Lerp( Lerp( y00, y10, xs ), Lerp( y01, y11, xs ), ys ), normalise, yOut );
    }
            
    void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        xs = InterpQuintic( x - xs );
        ys = InterpQuintic( y - ys );
        zs = InterpQuintic( z - zs );

    #define GRADIENT_COORD( _x, _y, _z )\
        int32v hash##_x##_y##_z = HashPrimesHB( seed, x##_x, y##_y, z##_z );\
        float32v x##_x##_y##_z = FS_Converti32_f32( hash##_x##_y##_z & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );\
        float32v y##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );\
        float32v z##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

        GRADIENT_COORD( 0, 0, 0 );
        GRADIENT_COORD( 1, 0, 0 );
        GRADIENT_COORD( 0, 1, 0 );
        GRADIENT_COORD( 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 1 );
        GRADIENT_COORD( 0, 1, 1 );
        GRADIENT_COORD( 1, 1, 1 );

    #undef GRADIENT_COORD

        float32v x0y = Lerp( Lerp( x000, x100, xs ), Lerp( x010, x110, xs ), ys );
        float32v y0y = Lerp( Lerp( y000, y100, xs ), Lerp( y010, y110, xs ), ys );
        float32v z0y = Lerp( Lerp( z000, z100, xs ), Lerp( z010, z110, xs ), ys );

        float32v x1y = Lerp( Lerp( x001, x101, xs ), Lerp( x011, x111, xs ), ys );
        float32v y1y = Lerp( Lerp( y001, y101, xs ), Lerp( y011, y111, xs ), ys );
        float32v z1y = Lerp( Lerp( z001, z101, xs ), Lerp( z011, z111, xs ), ys );

        float32v normalise = warpAmp * float32v( 1.0f / (0x3ff / 2.0f) );

        xOut = FS_FMulAdd_f32( Lerp( x0y, x1y, zs ), normalise, xOut );
        yOut = FS_FMulAdd_f32( Lerp( y0y, y1y, zs ), normalise, yOut );
        zOut = FS_FMulAdd_f32( Lerp( z0y, z1y, zs ), normalise, zOut );
    }
};

