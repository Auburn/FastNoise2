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
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &DomainScale::SetSource );
            this->AddVariable( "Scaling", 1.0f, &DomainScale::SetScaling );
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
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &DomainOffset::SetSource );
            this->AddPerDimensionHybridSource( "Offset", 0.0f, []( DomainOffset* p ) { return std::ref( p->mOffset ); } );
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
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &DomainRotate::SetSource );
            this->AddVariable( "Yaw", 0.0f, &DomainRotate::SetYaw );
            this->AddVariable( "Pitch", 0.0f, &DomainRotate::SetPitch );
            this->AddVariable( "Roll", 0.0f, &DomainRotate::SetRoll ); 
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

    protected:
        GeneratorSource mSource;
        HybridSource mFromMin = -1.0f;
        HybridSource mFromMax = 1.0f;
        HybridSource mToMin = 0.0f;
        HybridSource mToMax = 1.0f;

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
        }
    };
#endif

    class Terrace : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetMultiplier( float multiplier ) { mMultiplier = multiplier; mMultiplierRecip = 1 / multiplier; }
        void SetSmoothness( float smoothness ) { mSmoothness = smoothness; if( mSmoothness != 0.0f ) mSmoothnessRecip = 1 + 1 / smoothness; }

    protected:
        GeneratorSource mSource;
        float mMultiplier = 1.0f;
        float mMultiplierRecip = 1.0f;
        float mSmoothness = 0.0f;
        float mSmoothnessRecip = 0.0f;
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
            this->AddVariable( "Multiplier", 1.0f, &Terrace::SetMultiplier );
            this->AddVariable( "Smoothness", 0.0f, &Terrace::SetSmoothness );
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

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainAxisScale> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &DomainAxisScale::SetSource );
            this->AddPerDimensionVariable( "Scaling", 1.0f, []( DomainAxisScale* p ) { return std::ref( p->mScale ); } );
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
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &AddDimension::SetSource );
            this->AddHybridSource( "New Dimension Position", 0.0f, &AddDimension::SetNewDimensionPosition, &AddDimension::SetNewDimensionPosition );
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
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &RemoveDimension::SetSource );
            this->AddVariableEnum( "Remove Dimension", Dim::Y, &RemoveDimension::SetRemoveDimension, kDim_Strings );
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
        }
    };
#endif

    class SquareRoot : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }

    protected:
        GeneratorSource mSource;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SquareRoot> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Modifiers" );
            this->AddGeneratorSource( "Source", &SquareRoot::SetSource );
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
        }
    };
#endif
    
}
