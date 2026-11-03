#include "Fractal.h"

template<FastSIMD::FeatureSet SIMD, typename T>
class FastSIMD::DispatchClass<Fractal<T>, SIMD> : public virtual Fractal<T>, public DispatchClass<Generator, SIMD>
{
protected:
    // Block evaluation of FBm/Ridged with constant gain and weighted strength, giving the same values as their GenT:
    // the source is evaluated for the whole block once per octave. The fractal's noise value is the output of the source
    // through octaveValue( source ) (FBm: source, Ridged: 1 - 2 * |source|), weightValue( source ) is the value lerped
    // towards for weighted strength. Static taking the final class so members are read at fixed offsets
    template<size_t DIMS, typename Self, typename OctaveValue, typename WeightValue>
    static FASTNOISE_NOINLINE void GenOctaveBlocks( const Self& self, int32v seed, const float* pos, float* out, intptr_t count, OctaveValue octaveValue, WeightValue weightValue )
    {
        static constexpr intptr_t kBlockSize = DispatchClass<Generator, SIMD>::kGenBlockSize;
        static constexpr intptr_t N = int32v::ElementCount;

        alignas( 64 ) float octavePos[kBlockSize * DIMS];
        alignas( 64 ) float noiseBuffers[2][kBlockSize];
        alignas( 64 ) float amp[kBlockSize];

        float gain = self.mGain.constant;
        float lacunarity = self.mLacunarity;
        float weightedStrength = self.mWeightedStrength.constant;
        // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
        bool isWeighted = weightedStrength != 0.0f;
        int octaves = self.mOctaves;
        float constantAmp = 1.0f;

        self.template GetSourceBlock<DIMS>( self.mSource, seed, pos, noiseBuffers[0], count );

        for( intptr_t i = 0; i < count; i += N )
        {
            FS::Store( out + i, octaveValue( FS::Load<float32v>( noiseBuffers[0] + i ) ) );

            if( isWeighted )
            {
                FS::Store( amp + i, float32v( 1 ) );
            }
        }

        const float* srcPos = pos;

        for( int octave = 1; octave < octaves; octave++ )
        {
            const float* prevNoise = noiseBuffers[( octave - 1 ) & 1];
            float* noise = noiseBuffers[octave & 1];

            seed -= int32v( -1 );
            constantAmp *= gain;

            for( intptr_t i = 0; i < count * (intptr_t)DIMS; i += N )
            {
                FS::Store( octavePos + i, FS::Load<float32v>( srcPos + i ) * float32v( lacunarity ) );
            }
            srcPos = octavePos;

            self.template GetSourceBlock<DIMS>( self.mSource, seed, octavePos, noise, count );

            for( intptr_t i = 0; i < count; i += N )
            {
                float32v octaveAmp = float32v( constantAmp );

                if( isWeighted )
                {
                    octaveAmp = FS::Load<float32v>( amp + i );
                    octaveAmp *= Lerp( float32v( 1 ), weightValue( FS::Load<float32v>( prevNoise + i ) ), float32v( weightedStrength ) );
                    octaveAmp *= float32v( gain );
                    FS::Store( amp + i, octaveAmp );
                }

                float32v sum = FS::Load<float32v>( out + i );
                sum += octaveValue( FS::Load<float32v>( noise + i ) ) * octaveAmp;
                FS::Store( out + i, sum );
            }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FractalFBm, SIMD> final : public virtual FractalFBm, public DispatchClass<Fractal<>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v gain = this->GetSourceValue( mGain  , seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( 1.0f );
        float32v noise = this->GetSourceValue( mSource, seed, pos... );

        float32v sum = noise * amp;

        // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
        bool isWeighted = mWeightedStrength.simdGeneratorPtr || mWeightedStrength.constant != 0.0f;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            if( isWeighted )
            {
                amp *= Lerp( float32v( 1 ), (noise + float32v( 1 )) * float32v( 0.5f ), weightedStrength );
                amp *= gain;
            }
            else
            {
                amp *= gain;
            }

            noise = this->GetSourceValue( mSource, seed, (pos *= lacunarity)... );
            sum += noise * amp;
        }

        return sum;
    }

public:
    static constexpr bool kHasGenBlock = true;

    // Octave by octave over the whole block: one source block call per octave instead of a source call per vector
    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        if( mGain.simdGeneratorPtr || mWeightedStrength.simdGeneratorPtr )
        {
            return false;
        }

        this->template GenOctaveBlocks<DIMS>( *this, seed, pos, out, count,
            []( float32v noise ) { return noise; },
            []( float32v noise ) { return ( noise + float32v( 1 ) ) * float32v( 0.5f ); } );

        return true;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FractalRidged, SIMD> final : public virtual FractalRidged, public DispatchClass<Fractal<>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT(int32v seed, P... pos) const
    {
        float32v gain = this->GetSourceValue( mGain, seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( 1.0f );
        float32v noise = FS::Abs( this->GetSourceValue( mSource, seed, pos... ) );

        float32v sum = (noise * float32v( -2 ) + float32v( 1 )) * amp;

        // Lerp( 1, x, 0 ) == 1, skip it for the common constant 0 weighted strength
        bool isWeighted = mWeightedStrength.simdGeneratorPtr || mWeightedStrength.constant != 0.0f;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            if( isWeighted )
            {
                amp *= Lerp( float32v( 1 ), float32v( 1 ) - noise, weightedStrength );
                amp *= gain;
            }
            else
            {
                amp *= gain;
            }

            noise = FS::Abs( this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) );
            sum += (noise * float32v( -2 ) + float32v( 1 )) * amp;
        }

        return sum;
    }

public:
    static constexpr bool kHasGenBlock = true;

    // Octave by octave over the whole block, see FractalFBm
    template<size_t DIMS>
    FS_FORCEINLINE bool GenBlock( int32v seed, const float* pos, float* out, intptr_t count ) const
    {
        if( mGain.simdGeneratorPtr || mWeightedStrength.simdGeneratorPtr )
        {
            return false;
        }

        this->template GenOctaveBlocks<DIMS>( *this, seed, pos, out, count,
            []( float32v noise ) { return FS::Abs( noise ) * float32v( -2 ) + float32v( 1 ); },
            []( float32v noise ) { return float32v( 1 ) - FS::Abs( noise ); } );

        return true;
    }
};
