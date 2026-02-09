#pragma once
#include "Generator.h"

namespace FastNoise
{
    /** @brief Base class for generators that have a configurable spatial scale (feature size).
     *
     *  Scale controls the size of noise features in world units. A scale of 100 means
     *  one full noise period spans 100 units. Internally this is stored as both scale
     *  and its reciprocal (frequency = 1/scale) for efficient computation.
     *
     *  The "Feature Scale" parameter in the Node Editor corresponds to this value.
     */
    class ScalableGenerator : public virtual Generator
    {
    public:
        /** @brief Set the feature scale (effectively 1.0 / frequency).
         *  @param value  Scale in world units. Larger values produce larger features.
         */
        void SetScale( float value )
        {
            mScale = value;
            mFrequency = 1.0f / value;
        }

    protected:
        float mScale = 100;            ///< Feature scale in world units.
        float mFrequency = 1.0f / 100; ///< Reciprocal of scale (frequency).
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

    /** @brief Mixin that adds a seed offset to a generator.
     *
     *  The seed offset is added to the generation seed before this node evaluates,
     *  but does not propagate to child source nodes. This is useful when you have
     *  multiple nodes of the same type and want each to produce different output
     *  from the same base seed.
     *
     *  @tparam PARENT  The parent class in the inheritance chain.
     */
    template<typename PARENT>
    class Seeded : public virtual PARENT
    {
    public:
        /** @brief Set an offset that is added to the seed before generation.
         *  @param value  Seed offset. Does not affect seeds passed to child nodes.
         */
        void SetSeedOffset( int value )
        {
            mSeedOffset = value;
        }

    protected:
        int mSeedOffset = 0; ///< Offset added to the seed for this node only.
    };

#ifdef FASTNOISE_METADATA
    template<typename PARENT>
    struct MetadataT<Seeded<PARENT>> : MetadataT<PARENT>
    {
        MetadataT()
        {
            this->AddVariable( { "Seed Offset",
                "This value is added to the seed before generation\n"
                "Doesn't affect the seed passed to child nodes\n"
                "Useful if you have multiple nodes of the same type and want them to give different outputs" }, 0, &Seeded<PARENT>::SetSeedOffset );
        }
    };
#endif

    /** @brief Mixin that adds configurable output range scaling to a generator.
     *
     *  By default, coherent noise generators output values in the range [-1, 1].
     *  VariableRange allows remapping this to an arbitrary [min, max] range, which
     *  is applied internally during generation for maximum efficiency.
     *
     *  @tparam PARENT  The parent class in the inheritance chain.
     */
    template<typename PARENT>
    class VariableRange : public virtual PARENT
    {
    public:
        /** @brief Set the minimum bound of the output range.
         *  @param value  New minimum output value.
         */
        void SetOutputMin( float value )
        {
            mRangeScale += mRangeMin - value;
            mRangeMin = value;
        }

        /** @brief Set the maximum bound of the output range.
         *  @param value  New maximum output value.
         */
        void SetOutputMax( float value )
        {
            mRangeScale = ( value - mRangeMin );
        }

    protected:
        float mRangeMin = -1;   ///< Minimum of the output range (default -1).
        float mRangeScale = 2;  ///< Scale factor (max - min) for output mapping.
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

    class White : public virtual VariableRange<Seeded<Generator>>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<VariableRange<Seeded<Generator>>>
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

    class Gradient : public virtual Generator
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
    struct MetadataT<Gradient> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddPerDimensionVariable( { "Multiplier", "Read node description" }, 0.0f, []( Gradient* p ) { return std::ref( p->mMultiplier ); }, 0.f, 0.f, 0.001f );
            this->AddPerDimensionHybridSource( { "Offset", "Read node description" }, 0.0f, []( Gradient* p ) { return std::ref( p->mOffset ); }, 0.25f );

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
