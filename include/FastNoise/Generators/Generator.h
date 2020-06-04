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

        HybridSourceT( float f = 0.0f )
        {
            constant = f;
        }
    };

    class Generator
    {
    public:
        using Metadata = FastNoise::Metadata;

        virtual ~Generator() = default;

        virtual FastSIMD::eLevel GetSIMDLevel() const = 0;

        virtual void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) const = 0;
        virtual void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) const = 0;

        void GenUniformGrid2D( float* noiseOut, int32_t xStart, int32_t yStart, int32_t xSize, int32_t ySize, float frequency, int32_t seed )
        {
            GenUniformGrid2D( noiseOut, xStart * frequency, yStart * frequency, xSize, ySize, frequency, frequency, seed );
        }
        void GenUniformGrid3D( float* noiseOut, int32_t xStart, int32_t yStart, int32_t zStart, int32_t xSize, int32_t ySize, int32_t zSize, float frequency, int32_t seed )
        {
            GenUniformGrid3D( noiseOut, xStart * frequency, yStart * frequency, zStart * frequency, xSize, ySize, zSize, frequency, frequency, frequency, seed );
        }

        virtual void GenPositionArray3D( float* noiseOut, const float* xPosArray, const float* yPosArray, const float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) const = 0;     

        virtual const Metadata* GetMetadata() = 0;

    protected:
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
