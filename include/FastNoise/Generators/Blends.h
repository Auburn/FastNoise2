#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename LHS_T>
    class Operator : public virtual SourceStore<2>
    {
    protected:
        LHS_T mLHS;
        HybridSource<1> mRHS;

        FASTNOISE_METADATA_ABSTRACT( SourceStore<2> )
            using SourceStore<2>::Metadata::Metadata;
        };
    };

    using OperatorSourceLHS = Operator<GeneratorSource<0>>;
    using OperatorHybridLHS = Operator<HybridSource<0>>;

    class Add : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Subtract : public virtual OperatorHybridLHS
    {
        FASTNOISE_METADATA( OperatorHybridLHS )
            using OperatorHybridLHS::Metadata::Metadata;
        };    
    };

    class Multiply : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Divide : public virtual OperatorHybridLHS
    {
        FASTNOISE_METADATA( OperatorHybridLHS )
            using OperatorHybridLHS::Metadata::Metadata;
        };    
    };

    class Min : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Max : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Fade : public virtual SourceStore<3>
    {
    public:
        void SetFade( float value ) { mFade = value; }

    protected:
        GeneratorSource<0> mA;
        GeneratorSource<1> mB;
        HybridSource<2> mFade = 0.5f;

        FASTNOISE_METADATA( SourceStore<3> )

            Metadata( const char* className ) : SourceStore<3>::Metadata( className )
            {
                memberVariables.emplace_back( "Fade", 0.5f, &Fade::SetFade, 0.0f, 1.0f );
            }
        };    
    };
}
