#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Add : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Subtract : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Multiply : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Divide : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Min : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Max : public virtual Blend<2>
    {
        FASTNOISE_METADATA( Blend<2> )
            using Blend<2>::Metadata::Metadata;
        };    
    };

    class Fade : public virtual Blend<2>
    {
    public:
        void SetFade( float value ) { mFade = value; }

    protected:
        float mFade = 0.5f;

        FASTNOISE_METADATA( Blend<2> )

            Metadata( const char* className ) : Blend<2>::Metadata( className )
            {
                memberVariables.emplace_back( "Fade", 0.5f, &SetFade, 0.0f, 1.0f );
            }
        };    
    };
}
