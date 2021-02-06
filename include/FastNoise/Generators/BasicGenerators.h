#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Constant : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Constant> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Value", 1.0f, &Constant::SetValue );
        }
    };
#endif

    class White : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
        }
    };
#endif

    class Checkerboard : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSize( float value ) { mSize = value; }

    protected:
        float mSize = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Checkerboard> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Size", 1.0f, &Checkerboard::SetSize );
        }
    };
#endif

    class SineWave : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetScale( float value ) { mScale = value; }

    protected:
        float mScale = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SineWave> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Scale", 1.0f, &SineWave::SetScale );
        }
    };
#endif

    class PositionOutput : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        template<Dim D>
        void Set( float multiplier, float offset = 0.0f ) { mMultiplier[(int)D] = multiplier; mOffset[(int)D] = offset; }

    protected:
        PerDimensionVariable<float> mMultiplier;
        PerDimensionVariable<float> mOffset;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PositionOutput> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddPerDimensionVariable( "Multiplier", 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
            this->AddPerDimensionVariable( "Offset", 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );
        }
    };
#endif

    class DistanceToOrigin : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

    protected:
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DistanceToOrigin> : MetadataT<Generator>
    {
        Generator* NodeFactory( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableEnum( "Distance Function", DistanceFunction::Euclidean, &DistanceToOrigin::SetDistanceFunction, "Euclidean", "Euclidean Squared", "Manhattan", "Hybrid", "MaxAxis" );
        }
    };
#endif
}
