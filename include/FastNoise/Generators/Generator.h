#pragma once
#include <array>
#include <memory>

#include "FastNoise/FastNoise_Config.h"
#include "FastNoise/FastNoiseMetadata.h"

namespace FastNoise
{
    class Generator
    {
    public:
        using Metadata = FastNoise::Metadata;

        virtual ~Generator() {}

        virtual FastSIMD::eLevel GetSIMDLevel() = 0;

        virtual void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) = 0;
        virtual void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) = 0;

        virtual void GenPositionArray3D( float* noiseOut, const float* xPosArray, const float* yPosArray, const float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) = 0;     

        virtual const Metadata* GetMetadata() = 0;
    };


    template<size_t I>
    class BaseSource
    { };

    template<size_t I>
    class GeneratorSource : public BaseSource<I>
    { };

    template<size_t I>
    class HybridSource : public BaseSource<I>
    {
    public:
        float constant;

        HybridSource()
        {
            constant = 0.0f;
        }

        HybridSource( float f )
        {
            constant = f;
        }

        operator float()
        {
            return constant;
        }
    };

    template<size_t SOURCE_COUNT, typename T = Generator, typename P = Generator>
    class SourceStore : public virtual P
    {
    protected:
        template<size_t index>
        void SetSourceT( const BaseSource<index>&, const std::shared_ptr<T>& gen )
        {
            static_assert( index < SOURCE_COUNT );

            SetSourceImpl( gen, index );
        }

    private:      
        virtual void SetSourceImpl( const std::shared_ptr<T>& gen, size_t index = 0 ) = 0;

        FASTNOISE_METADATA_ABSTRACT( P )
        
            Metadata( const char* className ) : P::Metadata( className )
            {
                for( size_t i = 0; i < SOURCE_COUNT; i++ )
                {
                    this->memberNodes.emplace_back( "Source",
                        [i] ( SourceStore* g, std::shared_ptr<T> s )
                    {
                        g->SetSourceImpl( s, i );
                    });
                }
            }
        };
    };

    template<typename T = Generator, typename P = Generator>
    class SingleSource : public virtual SourceStore<1, T, P>
    {
    public:
        void SetSource( const std::shared_ptr<T>& gen )
        {
            this->SetSourceT( mSource, gen );
        }

    protected:
        GeneratorSource<0> mSource;

        FASTNOISE_METADATA_ABSTRACT( P )

            Metadata( const char* className ) : P::Metadata( className )
            {
                /*for( size_t i = 0; i < SOURCE_COUNT; i++ )
                {
                    this->memberNodes.emplace_back( "Source",
                        [i]( SourceStore* g, std::shared_ptr<T> s )
                        {
                            g->SetSourceImpl( s, i );
                        } );
                }*/
            }
        };
    };

    class Constant : public virtual Generator
    {
    public:
        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                memberVariables.emplace_back( "Value", 1.0f, &Constant::SetValue );
            }
        };    
    };
}
