#pragma once
#include <array>
#include <cassert>
#include <memory>

#include "FastNoise/FastNoise_Config.h"
#include "FastNoise/FastNoiseMetadata.h"

namespace FastNoise
{
    template<typename T>
    struct BaseSource
    {
        std::shared_ptr<T> base;
        void* simdGeneratorPtr = nullptr;

    protected:
        BaseSource() = default;
    };

    template<typename T>
    struct GeneratorSourceT : BaseSource<T>
    { };

    template<typename T>
    struct HybridSourceT : BaseSource<T>
    {
        float constant;

        HybridSourceT()
        {
            constant = 0.0f;
        }

        HybridSourceT( float f )
        {
            constant = f;
        }

        operator float()
        {
            return constant;
        }
    };

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


        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, const std::shared_ptr<T>& gen )
        {
            static_assert( std::is_base_of_v<Generator, T> );
            assert( gen.get() );
            assert( GetSIMDLevel() == gen->GetSIMDLevel() );

            memberVariable.base = gen;
            SetSourceSIMDPtr( dynamic_cast<Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
        }

    private:
        virtual void SetSourceSIMDPtr( Generator* base, void** simdPtr ) = 0;
    };

    using GeneratorSource = GeneratorSourceT<Generator>;
    using HybridSource = HybridSourceT<Generator>;


    class Constant : public virtual Generator
    {
    public:
        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddVariable( "Value", 1.0f, &Constant::SetValue );
            }
        };    
    };
}
