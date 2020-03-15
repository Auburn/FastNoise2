#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainScale : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        void SetScale( float value ) { mScale = value; };

    protected:
        float mScale = 1.0f;

        FASTNOISE_METADATA( Modifier<1> )
        
            Metadata( const char* className ) : Modifier<1>::Metadata( className )
            {

            }
        };    
    };

    class Remap : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        void SetRemap(float fromMin, float fromMax, float toMin, float toMax) { mFromMin = fromMin; mFromMax = fromMax; mToMin = toMin; mToMax = toMax; }

    protected:
        float mFromMin = -1.0f;
        float mFromMax = 1.0f;
        float mToMin = 0.0f;
        float mToMax = 1.0f;

        FASTNOISE_METADATA( Modifier<1> )
        
            Metadata( const char* className ) : Modifier<1>::Metadata( className )
            {
                memberVariables.emplace_back( "From Min", -1.0f,
                    []( Remap* p, float f )
                {
                    p->mFromMin = f;
                });
                
                memberVariables.emplace_back( "From Max", 1.0f,
                    []( Remap* p, float f )
                {
                    p->mFromMax = f;
                });
                
                memberVariables.emplace_back( "To Min", 0.0f,
                    []( Remap* p, float f )
                {
                    p->mToMin = f;
                });

                memberVariables.emplace_back( "To Max", 1.0f,
                    std::function<void(Remap*,float)>( []( Remap* p, float f )
                {
                    p->mToMax = f;
                })); //std function to test compile, temp
            }
        };    
    };

    class ConvertRGBA8 : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT(FastNoise::SUPPORTED_SIMD_LEVELS);

        void SetMinMax(float min, float max) { mMin = min; mMax = max; }

    protected:
        float mMin = -1.0f;
        float mMax = 1.0f;

        FASTNOISE_METADATA( Modifier<1> )
        
            Metadata( const char* className ) : Modifier<1>::Metadata( className )
            {

            }
        };    
    };
}
