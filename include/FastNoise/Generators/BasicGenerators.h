#pragma once
#include "Generator.h"

namespace FastNoise
{
    class ScalableGenerator : public virtual Generator
    {
    public:
        void SetScale( float value )
        {
            mScale = value;
            mFrequency = 1.0f / value;
        }

    protected:
        float mScale = 100;
        float mFrequency = 1.0f / 100;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<ScalableGenerator> : MetadataT<Generator>
    {
        MetadataT()
        {
            this->AddVariable( "Feature Scale", 100.0f, &ScalableGenerator::SetScale, 0.f, 0.f, 0.25f );
        }
    };
#endif

    class Constant : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Constant> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

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
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
        }
    };
#endif

    class Checkerboard : public virtual ScalableGenerator
    {
    public:
        const Metadata& GetMetadata() const override;
        
        void SetHigh( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mHigh, gen ); }
        void SetHigh( float value ) { mHigh = value; }
        void SetLow( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLow, gen ); }
        void SetLow( float value ) { mLow = value; }

    protected:
        HybridSource mHigh = 1.0f;
        HybridSource mLow = -1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Checkerboard> : MetadataT<ScalableGenerator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddHybridSource( "High", 1.0f, &Checkerboard::SetHigh, &Checkerboard::SetHigh );
            this->AddHybridSource( "Low", -1.0f, &Checkerboard::SetLow, &Checkerboard::SetLow );
        }
    };
#endif

    class SineWave : public virtual ScalableGenerator
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SineWave> : MetadataT<ScalableGenerator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
        }
    };
#endif

    class PositionOutput : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        template<Dim D>
        void SetAxis( float multiplier, float offset = 0.0f ) { mMultiplier[(int)D] = multiplier; mOffset[(int)D] = offset; }

    protected:
        PerDimensionVariable<float> mMultiplier = 0.0f;
        PerDimensionVariable<float> mOffset = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PositionOutput> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddPerDimensionVariable( "Multiplier", 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
            this->AddPerDimensionVariable( "Offset", 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );
        }
    };
#endif

    class DistanceToPoint : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

        template<Dim D>
        void SetScale( float value ) { mPoint[(int)D] = value; }

    protected:
        GeneratorSource mSource;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;
        PerDimensionVariable<float> mPoint = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DistanceToPoint> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableEnum( "Distance Function", DistanceFunction::Euclidean, &DistanceToPoint::SetDistanceFunction, kDistanceFunction_Strings );
            this->AddPerDimensionVariable( "Point", 0.0f, []( DistanceToPoint* p ) { return std::ref( p->mPoint ); } );
        }
    };
#endif
}
