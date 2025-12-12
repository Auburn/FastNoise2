#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainScale : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetScaling( float value ) { mScale = value; }

    protected:
        GeneratorSource mSource;
        float mScale = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainScale> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &DomainScale::SetSource );
            this->AddVariable( "Scaling", 1.0f, &DomainScale::SetScaling );

            description =
                "Scales the input coordinates uniformly before passing them to the source generator.";
        }
    };
#endif

    class DomainOffset : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

        template<Dim D>
        void SetOffset( float value ) { mOffset[(int)D] = value; }

        template<Dim D>
        void SetOffset( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mOffset[(int)D], gen ); }

    protected:
        GeneratorSource mSource;
        PerDimensionVariable<HybridSource> mOffset = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainOffset> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &DomainOffset::SetSource );
            this->AddPerDimensionHybridSource( "Offset", 0.0f, []( DomainOffset* p ) { return std::ref( p->mOffset ); }, 0.25f );

            description =
                "Adds an offset to the input coordinates before passing them to the source generator\n"
                "Doesn't add or remove dimensions";
        }
    };
#endif

    class DomainRotate : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

        void SetYaw(   float value ) { mYawCos   = std::cos( value ); mYawSin   = std::sin( value ); CalculateRotation(); }
        void SetPitch( float value ) { mPitchCos = std::cos( value ); mPitchSin = std::sin( value ); CalculateRotation(); }
        void SetRoll(  float value ) { mRollCos  = std::cos( value ); mRollSin  = std::sin( value ); CalculateRotation(); }

    protected:
        GeneratorSource mSource;
        float mYawCos   = 1.0f;
        float mYawSin   = 0.0f;
        float mPitchCos = 1.0f;
        float mPitchSin = 0.0f;
        float mRollCos  = 1.0f;
        float mRollSin  = 0.0f;

        float mXa = 1.0f;
        float mXb = 0.0f;
        float mXc = 0.0f;
        float mYa = 0.0f;
        float mYb = 1.0f;
        float mYc = 0.0f;
        float mZa = 0.0f;
        float mZb = 0.0f;
        float mZc = 1.0f;

        void CalculateRotation()
        {
            mXa = mYawCos * mPitchCos;
            mXb = mYawCos * mPitchSin * mRollSin - mYawSin * mRollCos;
            mXc = mYawCos * mPitchSin * mRollCos + mYawSin * mRollSin;

            mYa = mYawSin * mPitchCos;
            mYb = mYawSin * mPitchSin * mRollSin + mYawCos * mRollCos;
            mYc = mYawSin * mPitchSin * mRollCos - mYawCos * mRollSin;

            mZa = -mPitchSin;
            mZb = mPitchCos * mRollSin;
            mZc = mPitchCos * mRollCos;
        }
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainRotate> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &DomainRotate::SetSource );
            this->AddVariable( "Yaw", 0.0f, &DomainRotate::SetYaw );
            this->AddVariable( "Pitch", 0.0f, &DomainRotate::SetPitch );
            this->AddVariable( "Roll", 0.0f, &DomainRotate::SetRoll ); 

            description =
                "Rotates the input coordinates around the origin before passing them to the source generator\n"
                "For 2D input coordinates a 2D rotation with Yaw is performed if Pitch and Roll are 0, otherwise a 3D rotation is performed\n"
                "For 3D input coordinates a 3D rotation is performed\n"
                "For 4D input coordinates no rotation is applied";
        }
    };
#endif

    class SeedOffset : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetOffset( int value ) { mOffset = value; }

    protected:
        GeneratorSource mSource;
        int mOffset = 1;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SeedOffset> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &SeedOffset::SetSource );
            this->AddVariable( "Seed Offset", 1, &SeedOffset::SetOffset );

            description =
                "Offsets the input seed before passing it to the source generator.";
        }
    };
#endif

    class Remap : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        
        void SetFromMin( float value ) { mFromMin = value; }
        void SetFromMin( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFromMin, gen ); }
        
        void SetFromMax( float value ) { mFromMax = value; }
        void SetFromMax( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFromMax, gen ); }
        
        void SetToMin( float value ) { mToMin = value; }
        void SetToMin( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mToMin, gen ); }
        
        void SetToMax( float value ) { mToMax = value; }
        void SetToMax( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mToMax, gen ); }

        void SetClampOutput( Boolean value ) { mClampOutput = value; }
        void SetClampOutput( bool value ) { mClampOutput = value ? Boolean::True : Boolean::False; }

    protected:
        GeneratorSource mSource;
        HybridSource mFromMin = -1.0f;
        HybridSource mFromMax = 1.0f;
        HybridSource mToMin = 0.0f;
        HybridSource mToMax = 1.0f;
        Boolean mClampOutput = Boolean::False;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Remap> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &Remap::SetSource );
            
            this->AddHybridSource( "From Min", -1.0f, &Remap::SetFromMin, &Remap::SetFromMin );
            this->AddHybridSource( "From Max", 1.0f, &Remap::SetFromMax, &Remap::SetFromMax );
            this->AddHybridSource( "To Min", 0.0f, &Remap::SetToMin, &Remap::SetToMin );
            this->AddHybridSource( "To Max", 1.0f, &Remap::SetToMax, &Remap::SetToMax );
            this->AddVariableEnum<Boolean>( { "Clamp Output", "Clamp output between \"To Min\" & \"To Max\"" }, Boolean::False, &Remap::SetClampOutput, kBoolean_Strings );

            description =
                "Remaps the output value of the source generator from one range to another\n"
                "Optionally clamps output to the To Min/Max range";
        }
    };
#endif

    class ConvertRGBA8 : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetMinMax( float min, float max ) { mMin = min; mMax = max; }

    protected:
        GeneratorSource mSource;
        float mMin = -1.0f;
        float mMax = 1.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<ConvertRGBA8> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &ConvertRGBA8::SetSource );

            this->AddVariable( "Min", -1.0f,
                []( ConvertRGBA8* p, float f )
                {
                    p->mMin = f;
                } );

            this->AddVariable( "Max", 1.0f,
                []( ConvertRGBA8* p, float f )
                {
                    p->mMax = f;
                } );

            description =
                "Used for converting a float into a greyscale RGBA8 texture format output\n"
                "Clamps the source output between Min/Max, scales it to 0-255, and packs the result\n"
                "into an RGBA8 color stored in a float. RGB will be the same value, Alpha is always 255";
        }
    };
#endif

    class Terrace : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetStepCount( float stepCount ) { mStepCount = stepCount; mStepCountRecip = 1 / stepCount; }
        
        void SetSmoothness( float smoothness ) { mSmoothness = smoothness; if( smoothness != 0.0f ) mSmoothnessRecip = 1 + 1 / smoothness; }
        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }

    protected:
        GeneratorSource mSource;
        HybridSource mSmoothness = 0.0f;
        float mSmoothnessRecip = 0.0f;
        float mStepCount = 1.0f;
        float mStepCountRecip = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Terrace> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &Terrace::SetSource );
            this->AddVariable( { "Step Count", "Increasing the step count reduces the size of each step" }, 1.0f, &Terrace::SetStepCount );
            this->AddHybridSource( { "Smoothness", "How smooth the transitions between steps are" }, 0.0f, &Terrace::SetSmoothness, &Terrace::SetSmoothness );

            description =
                "Cuts the input value into steps to give a terraced terrain effect";
        }
    };
#endif

    class PingPong : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

        void SetPingPongStrength( float value ) { mPingPongStrength = value; }
        void SetPingPongStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPingPongStrength, gen ); }

    protected:
        GeneratorSource mSource;
        HybridSource mPingPongStrength = 2.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PingPong> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &PingPong::SetSource );
            this->AddHybridSource( { "Ping Pong Strength",
                "Controls how dramatically values bounce between extremes\n"
                "Higher values create more pronounced ping-pong effects" },
                2.0f, &PingPong::SetPingPongStrength, &PingPong::SetPingPongStrength );

            description =
                "Creates flow patterns by 'ping-ponging' input values between extremes\n"
                "Multiplies input values by the ping pong strength and reflects values\n"
                "that would normally go beyond -1.0 or 1.0 and bounce back instead.\n"
                "Produces smooth, flowing patterns similar to contour lines";
        }
    };
#endif

    class DomainAxisScale : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

        template<Dim D>
        void SetScaling( float value ) { mScale[(int)D] = value; }

    protected:
        GeneratorSource mSource;
        PerDimensionVariable<float> mScale = 1.0f;

        friend struct MetadataT<DomainAxisScale>;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainAxisScale> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &DomainAxisScale::SetSource );
            this->AddPerDimensionVariable( "Scaling", 1.0f, []( DomainAxisScale* p ) { return std::ref( p->mScale ); } );

            description =
                "Scales each axis of the input coordinates independently before passing them to the source generator.";
        }
    };
#endif

    class AddDimension : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetNewDimensionPosition( float value ) { mNewDimensionPosition = value; }
        void SetNewDimensionPosition( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mNewDimensionPosition, gen ); }

    protected:
        GeneratorSource mSource;
        HybridSource mNewDimensionPosition = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<AddDimension> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &AddDimension::SetSource );
            this->AddHybridSource( { "New Dimension Position", "The position of the new dimension" }, 0.0f, &AddDimension::SetNewDimensionPosition, &AddDimension::SetNewDimensionPosition );

            description =
                "Adds a dimension to the input coordinates, new dimension is always the last dimension\n"
                "The coordinates with the new dimension are passed to the source generator";
        }
    };
#endif

    class RemoveDimension : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetRemoveDimension( Dim dimension ) { mRemoveDimension = dimension; }

    protected:
        GeneratorSource mSource;
        Dim mRemoveDimension = Dim::Y;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<RemoveDimension> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &RemoveDimension::SetSource );
            this->AddVariableEnum( "Remove Dimension", Dim::Y, &RemoveDimension::SetRemoveDimension, kDim_Strings );

            description =
                "Removes the specified dimension from the input coordinates before passing them to the source generator";
        }
    };
#endif

    class GeneratorCache : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

    protected:
        GeneratorSource mSource;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<GeneratorCache> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &GeneratorCache::SetSource );

            description =
                "Caches the output of the source generator. If the same input coordinates and seed are\n"
                "requested again, the cached value is returned, improving performance for complex source generators";
        }
    };
#endif

    class SignedSquareRoot : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

    protected:
        GeneratorSource mSource;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SignedSquareRoot> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &SignedSquareRoot::SetSource );

            description =
                "Returns the square root of the absolute value of the source output,\n"
                "preserving the original sign (signed square root).";
        }
    };
#endif

    class Abs : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

    protected:
        GeneratorSource mSource;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Abs> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &Abs::SetSource );

            description =
                "Returns the absolute value of the source output.";
        }
    };
#endif

    enum class PlaneRotationType
    {
        ImproveXYPlanes,
        ImproveXZPlanes
    };

    class DomainRotatePlane : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetRotationType( PlaneRotationType type ) { mRotationType = type; }

    protected:
        GeneratorSource mSource;
        PlaneRotationType mRotationType = PlaneRotationType::ImproveXYPlanes;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainRotatePlane> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Domain Modifiers" );
            this->AddGeneratorSource( "Source", &DomainRotatePlane::SetSource );
            
            this->AddVariableEnum( "Rotation Type", PlaneRotationType::ImproveXYPlanes, &DomainRotatePlane::SetRotationType, "Improve XY Planes", "Improve XZ Planes" );

            description =
                "Applies preset rotation to improve noise in specific 3D planes. Faster than DomainRotate.\n"
                "This helps reduce axis aligned artifacts in 3D noise for the specified plane.\n"
                "For 2D input coordinates a 3rd dimension is added and the noise is optimized for the XY plane\n"
                "For 4D input coordinates only the first 3 dimensions are rotated, the 4th dimension is passed through unchanged";
        }
    };
#endif
    
}
