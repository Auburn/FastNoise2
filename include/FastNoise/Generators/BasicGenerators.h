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
            this->AddVariable( { "Feature Scale", "Effectively `1.0 / frequency`" }, 100.0f, &ScalableGenerator::SetScale, 0.f, 0.f, 0.25f );
        }
    };
#endif

    template<typename PARENT>
    class VariableRange : public virtual PARENT
    {
    public:
        void SetOutputMin( float value )
        {
            mRangeScale += mRangeMin - value;
            mRangeMin = value;
        }

        void SetOutputMax( float value )
        {
            mRangeScale = ( value - mRangeMin );
        }

    protected:
        float mRangeMin = -1;
        float mRangeScale = 2;
    };

#ifdef FASTNOISE_METADATA
    template<typename PARENT>
    struct MetadataT<VariableRange<PARENT>> : MetadataT<PARENT>
    {
        MetadataT()
        {
            this->AddVariable( { "Output Min", "Minimum bound of output range" }, -1.0f, &VariableRange<PARENT>::SetOutputMin );
            this->AddVariable( { "Output Max", "Maximum bound of output range" }, 1.0f, &VariableRange<PARENT>::SetOutputMax );
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
            this->AddVariable( { "Value", "Constant output" }, 1.0f, &Constant::SetValue );
        }
    };
#endif

    class White : public virtual VariableRange<Generator>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<VariableRange<Generator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            
            description = 
                "White noise generator";
        }
    };
#endif

    class Checkerboard : public virtual VariableRange<ScalableGenerator>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Checkerboard> : MetadataT<VariableRange<ScalableGenerator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            description =
                "Outputs checkerboard pattern";
        }
    };
#endif

    class SineWave : public virtual VariableRange<ScalableGenerator>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SineWave> : MetadataT<VariableRange<ScalableGenerator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );

            description =
                "Outputs sine wave";
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
            this->AddPerDimensionVariable( { "Multiplier", "Read node description" }, 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
            this->AddPerDimensionVariable( { "Offset", "Read node description" }, 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );

            description =
                "Takes the input position and does the following per dimension\n"
                "(input + offset) * multiplier\n"
                "The output is the sum of all results";
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
            this->AddPerDimensionVariable( { "Point", "Point in current domain space" }, 0.0f, []( DistanceToPoint* p ) { return std::ref( p->mPoint ); } );

            description =
                "Outputs calculated distance between point and input position";
        }
    };
#endif
}
