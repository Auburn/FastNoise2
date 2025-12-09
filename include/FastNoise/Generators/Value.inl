#include "Value.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Value, SIMD> final : public virtual Value, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );

        return this->ScaleOutput( Lerp(
            Lerp( GetValueCoord( seed, x0, y0 ), GetValueCoord( seed, x1, y0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1 ), GetValueCoord( seed, x1, y1 ), xs ), ys ),
            -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y, z );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v z0 = FS::Convert<int32_t>( zs ) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );

        return this->ScaleOutput( Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0 ), GetValueCoord( seed, x1, y0, z0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0 ), GetValueCoord( seed, x1, y1, z0 ), xs ), ys ),
            Lerp(                                                                                
            Lerp( GetValueCoord( seed, x0, y0, z1 ), GetValueCoord( seed, x1, y0, z1 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1 ), GetValueCoord( seed, x1, y1, z1 ), xs ), ys ), zs ),
            -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( x, y, z, w );

        float32v xs = FS::Floor( x );
        float32v ys = FS::Floor( y );
        float32v zs = FS::Floor( z );
        float32v ws = FS::Floor( w );

        int32v x0 = FS::Convert<int32_t>( xs ) * int32v( Primes::X );
        int32v y0 = FS::Convert<int32_t>( ys ) * int32v( Primes::Y );
        int32v z0 = FS::Convert<int32_t>( zs ) * int32v( Primes::Z );
        int32v w0 = FS::Convert<int32_t>( ws ) * int32v( Primes::W );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );
        int32v w1 = w0 + int32v( Primes::W );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );
        ws = InterpHermite( w - ws );

        return this->ScaleOutput( Lerp( Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0, w0 ), GetValueCoord( seed, x1, y0, z0, w0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0, w0 ), GetValueCoord( seed, x1, y1, z0, w0 ), xs ), ys ),
            Lerp( 
            Lerp( GetValueCoord( seed, x0, y0, z1, w0 ), GetValueCoord( seed, x1, y0, z1, w0 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1, w0 ), GetValueCoord( seed, x1, y1, z1, w0 ), xs ), ys ), zs ),
            Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0, w1 ), GetValueCoord( seed, x1, y0, z0, w1 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0, w1 ), GetValueCoord( seed, x1, y1, z0, w1 ), xs ), ys ),
            Lerp( 
            Lerp( GetValueCoord( seed, x0, y0, z1, w1 ), GetValueCoord( seed, x1, y0, z1, w1 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1, w1 ), GetValueCoord( seed, x1, y1, z1, w1 ), xs ), ys ), zs ), ws ),
            -kValueBounds, kValueBounds );
    }
};
