#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename T = Generator>
    class Fractal : public virtual Generator
    {
    public:
        void SetSource( SmartNodeArg<T> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetGain( float value ) { mGain = value; }
        void SetGain( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mGain, gen ); }
        void SetWeightedStrength( float value ) { mWeightedStrength = value; } 
        void SetWeightedStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWeightedStrength, gen ); }
        void SetOctaveCount( int value ) { mOctaves = value; }
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:
        GeneratorSourceT<T> mSource;
        HybridSource mGain = 0.5f;
        HybridSource mWeightedStrength = 0.0f;

        int   mOctaves = 3;
        float mLacunarity = 2.0f;
    };

#ifdef FASTNOISE_METADATA
    template<typename T>
    struct MetadataT<Fractal<T>> : MetadataT<Generator>
    {
        MetadataT( NameDesc sourceName = "Source", bool addGroup = true )
        {
            if( addGroup )
            {
                groups.push_back( "Fractal" );
            }
            this->AddGeneratorSource( sourceName, &Fractal<T>::SetSource );
            this->AddHybridSource( { "Gain",
                "Multiplier for how much each octave contributes to the final result\n"
                "Lower values create smoother, more uniform noise\n"
                "Higher values preserve more detail from smaller octaves" },
                0.5f, &Fractal<T>::SetGain, &Fractal<T>::SetGain );

            this->AddHybridSource( { "Weighted Strength",
                "Scales the contribution of higher octaves based on the output value of previous octaves\n"
                "Low values from lower octaves reduces contribution from higher octaves\n"
                "Setting this higher creates smoother output in low areas while retaining octave details in high areas" },
                0.0f, &Fractal<T>::SetWeightedStrength, &Fractal<T>::SetWeightedStrength );

            this->AddVariable( { "Octaves",
                "Number of noise layers combined together\n"
                "More octaves = more detail but slower generation\n"
                "Each octave adds finer detail at higher frequencies" },
                3, &Fractal<T>::SetOctaveCount, 2, 16 );

            this->AddVariable( { "Lacunarity",
                "How much the frequency increases between octaves\n"
                "2.0 = Each octave is twice as detailed as the previous\n"
                "Higher values create more contrast between large and small features" },
                2.0f, &Fractal<T>::SetLacunarity );
        }
    };
#endif

    class FractalFBm : public virtual Fractal<>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<FractalFBm> : MetadataT<Fractal<>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
        
        MetadataT() : MetadataT<Fractal<>>()
        {
            description = 
                "Fractional Brownian Motion - Builds up realistic natural textures\n"
                "Combines multiple octaves where each adds progressively finer detail\n"
                "Creates smooth, flowing patterns ideal for terrain, clouds, and organic textures\n"
                "Each octave's contribution decreases by the Gain amount";
        }
    };
#endif

    class FractalRidged : public virtual Fractal<>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<FractalRidged> : MetadataT<Fractal<>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
        
        MetadataT() : MetadataT<Fractal<>>()
        {
            description = 
                "Creates sharp ridges and valleys by inverting and combining noise octaves\n"
                "Produces dramatic mountain-like terrain with defined peaks and canyons\n"
                "Higher octaves sharpen the ridges rather than adding smooth detail\n"
                "Perfect for creating realistic mountain ranges and rocky formations";
        }
    };
#endif
}
