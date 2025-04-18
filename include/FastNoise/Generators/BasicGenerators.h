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

            description =
                "Outputs a constant value";
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
                "Outputs a checkerboard pattern\n"
                "Each checkerboard cell is \"Feature Scale\" sized in each dimension";
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
        void SetMultiplier( float multiplier ) { mMultiplier[(int)D] = multiplier; }
        template<Dim D>
        void SetOffset( float offset ) { mOffset[(int)D] = offset; }
        template<Dim D>
        void SetOffset( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mOffset[(int)D], gen ); }

    protected:
        PerDimensionVariable<float> mMultiplier = 0.0f;
        PerDimensionVariable<HybridSource> mOffset = 0.0f;

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
            this->AddPerDimensionVariable( { "Multiplier", "Read node description" }, 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); }, 0.f, 0.f, 0.001f );
            this->AddPerDimensionHybridSource( { "Offset", "Read node description" }, 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); }, 0.25f );

            description =
                "Takes the input position and does the following per dimension\n"
                "`(input + offset) * multiplier`\n"
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

        void SetMinkowskiP( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mMinkowskiP, gen ); }
        void SetMinkowskiP( float value ) { mMinkowskiP = value; }

        void SetPoint( float x, float y, float z = 0, float w = 0 )
        {
            mPoint[0] = x;
            mPoint[1] = y;
            mPoint[2] = z;
            mPoint[3] = w;
        }

        template<Dim D>
        void SetPoint( float value ) { mPoint[(int)D] = value; }

        template<Dim D>
        void SetPoint( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPoint[(int)D], gen ); }

    protected:
        GeneratorSource mSource;
        HybridSource mMinkowskiP = 1.5f;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;
        PerDimensionVariable<HybridSource> mPoint = 0.0f;

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
            this->AddPerDimensionHybridSource( { "Point", "Point in current domain space" }, 0.0f, []( DistanceToPoint* p ) { return std::ref( p->mPoint ); } );

            this->AddHybridSource( { "Minkowski P", "Only affects Minkowski distance function\n1 = Manhattan\n2 = Euclidean" }, 1.5f, &DistanceToPoint::SetMinkowskiP, &DistanceToPoint::SetMinkowskiP );

            description =
                "Outputs distance between point and input position\n"
                "Distance is calculated in current domain space,\n"
                "ie affected by Domain Modifiers/Warping";
        }
    };
#endif
}
