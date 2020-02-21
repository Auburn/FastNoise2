#pragma once

#include <array>
#include <memory>

#include "../FastSIMD/FastSIMD.h"

#ifdef FS_SIMD_CLASS
#pragma warning( disable:4250 )
#endif

namespace FastNoise
{
    class Generator
    {
    public:
        virtual ~Generator() {}

        virtual void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) = 0;
        virtual void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) = 0;

        virtual void GenPositionArray3D( float* noiseOut, const float* xPosArray, const float* yPosArray, const float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) = 0;        
    };

    template<size_t SOURCE_COUNT>
    class Modifier : public virtual Generator
    {
    public:
        virtual void SetSource( const std::shared_ptr<Generator>& gen, size_t index = 0 ) = 0;

    protected:
        std::array<std::shared_ptr<Generator>, SOURCE_COUNT> mSourceBase;
    };

}
