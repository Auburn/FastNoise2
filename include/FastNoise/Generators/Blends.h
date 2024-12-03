#pragma once
#include "Generator.h"

#include <climits>

namespace FastNoise
{
    class OperatorSourceLHS : public virtual Generator
    {
    public:
        void SetLHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLHS, gen ); }
        void SetRHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mRHS, gen ); }
        void SetRHS( float value ) { mRHS = value; }

    protected:
        GeneratorSource mLHS;
        HybridSource mRHS = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OperatorSourceLHS> : MetadataT<Generator>
    {
        MetadataT( const char* group = "Blends" )
        {
            groups.push_back( group );
            this->AddGeneratorSource( "LHS", &OperatorSourceLHS::SetLHS );
            this->AddHybridSource( "RHS", 0.0f, &OperatorSourceLHS::SetRHS, &OperatorSourceLHS::SetRHS );
        }
    };
#endif

    class OperatorHybridLHS : public virtual Generator
    {
    public:
        void SetLHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLHS, gen ); }
        void SetLHS( float value ) { mLHS = value; }
        void SetRHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mRHS, gen ); }
        void SetRHS( float value ) { mRHS = value; }

    protected:
        HybridSource mLHS = 0.0f;
        HybridSource mRHS = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OperatorHybridLHS> : MetadataT<Generator>
    {
        MetadataT( const char* group = "Blends" )
        {
            groups.push_back( group );
            this->AddHybridSource( "LHS", 0.0f, &OperatorHybridLHS::SetLHS, &OperatorHybridLHS::SetLHS );
            this->AddHybridSource( "RHS", 0.0f, &OperatorHybridLHS::SetRHS, &OperatorHybridLHS::SetRHS );
        }
    };
#endif

    class Add : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Add> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<OperatorSourceLHS>( "Operators" ) {}
    };
#endif

    class Subtract : public virtual OperatorHybridLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Subtract> : MetadataT<OperatorHybridLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<OperatorHybridLHS>( "Operators" ) {}
    };
#endif

    class Multiply : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Multiply> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<OperatorSourceLHS>( "Operators" ) {}
    };
#endif

    class Divide : public virtual OperatorHybridLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Divide> : MetadataT<OperatorHybridLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<OperatorHybridLHS>( "Operators" ) {}
    };
#endif

    class Modulus : public virtual OperatorHybridLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Modulus> : MetadataT<OperatorHybridLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<OperatorHybridLHS>( "Operators" ) {}
    };
#endif

    class Min : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Min> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
    };
#endif

    class Max : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Max> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
    };
#endif

    class PowFloat : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetValue( float value ) { mValue = value; }
        void SetPow( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPow, gen ); }
        void SetPow( float value ) { mPow = value; }

    protected:
        HybridSource mValue = 2.0f;
        HybridSource mPow = 2.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PowFloat> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddHybridSource( "Value", 2.0f, &PowFloat::SetValue, &PowFloat::SetValue );
            this->AddHybridSource( "Pow", 2.0f, &PowFloat::SetPow, &PowFloat::SetPow );

            description = "Equivalent to std::powf( value, pow )";
        }
    };
#endif

    class PowInt : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetPow( int value ) { mPow = value; }

    protected:
        GeneratorSource mValue;
        int mPow = 2;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PowInt> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddGeneratorSource( "Value", &PowInt::SetValue );
            this->AddVariable( "Pow", 2, &PowInt::SetPow, 2 );

            description = "Faster than PowFloat node but only for int powers";
        }
    };
#endif

    class MinSmooth : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<MinSmooth> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            this->AddHybridSource( "Smoothness", 0.1f, &MinSmooth::SetSmoothness, &MinSmooth::SetSmoothness );

            description = 
                "Quadratic Smooth Minimum\n"
                "Smoothes the transition between the 2 inputs\n"
                "For explanation see:\n"
                "https://iquilezles.org/articles/smin/";
        }
    };
#endif

    class MaxSmooth : public virtual OperatorSourceLHS
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;  
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<MaxSmooth> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            this->AddHybridSource( "Smoothness", 0.1f, &MaxSmooth::SetSmoothness, &MaxSmooth::SetSmoothness );

            description =
                "Quadratic Smooth Maximum\n"
                "Smoothes the transition between the 2 inputs\n"
                "For explanation see:\n"
                "https://iquilezles.org/articles/smin/";
        }
    };
#endif

    class Fade : public virtual Generator
    {
    public:
        enum class Interpolation
        {
            Linear,
            Hermite,
            Quintic,
        };

        const Metadata& GetMetadata() const override;
        void SetA( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mA, gen ); }
        void SetB( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mB, gen ); }

        void SetFade( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFade, gen ); }
        void SetFade( float value ) { mFade = value; }

        void SetFadeMin( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFadeMin, gen ); }
        void SetFadeMin( float value ) { mFadeMin = value; }

        void SetFadeMax( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFadeMax, gen ); }
        void SetFadeMax( float value ) { mFadeMax = value; }

        void SetInterpolation( Interpolation interpolation ) { mInterpolation = interpolation; }

    protected:
        GeneratorSource mA;
        GeneratorSource mB;
        HybridSource mFade = 0;
        HybridSource mFadeMin = -1.f;
        HybridSource mFadeMax = 1.f;
        Interpolation mInterpolation = Interpolation::Linear;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Fade> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddGeneratorSource( { "A", "From" }, &Fade::SetA );
            this->AddGeneratorSource( { "B", "To" }, &Fade::SetB );
            this->AddHybridSource( "Fade", 0, &Fade::SetFade, &Fade::SetFade );
            this->AddHybridSource( "Fade Min", -1.f, &Fade::SetFadeMin, &Fade::SetFadeMin );
            this->AddHybridSource( "Fade Max", 1.f, &Fade::SetFadeMax, &Fade::SetFadeMax );
            this->AddVariableEnum( { "Interpolation", "Easing function" }, Fade::Interpolation::Linear, &Fade::SetInterpolation, "Linear", "Hermite", "Quintic" );            

            description =
                "Output fades between inputs A and B\n"
                "Fade Min = 100% A\n"
                "Fade Max = 100% B";
        }
    };
#endif
}
