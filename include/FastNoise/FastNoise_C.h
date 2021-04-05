#ifndef FASTNOISE_C_H
#define FASTNOISE_C_H

#include "FastNoise_Export.h"

#ifdef __cplusplus
extern "C" {
#endif

FASTNOISE_API void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned /*FastSIMD::eLevel*/ simdLevel /*0 = Auto*/ );

FASTNOISE_API void fnDeleteGeneratorRef( void* generator );

FASTNOISE_API unsigned fnGetSIMDLevel( const void* generator );

FASTNOISE_API void fnGenUniformGrid2D( const void* generator, float* noiseOut,
                                       int xStart, int yStart,
                                       int xSize, int ySize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenUniformGrid3D( const void* generator, float* noiseOut,
                                       int xStart, int yStart, int zStart,
                                       int xSize, int ySize, int zSize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenUniformGrid4D( const void* generator, float* noiseOut,
                                       int xStart, int yStart, int zStart, int wStart,
                                       int xSize, int ySize, int zSize, int wSize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray2D( const void* generator, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray,
                                         float xOffset, float yOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray3D( const void* generator, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray,
                                         float xOffset, float yOffset, float zOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray4D( const void* generator, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray,
                                         float xOffset, float yOffset, float zOffset, float wOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenTileable2D( const void* generator, float* noiseOut,
                                    int xSize, int ySize,
                                    float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API float fnGenSingle2D( const void* generator, float x, float y, int seed );
FASTNOISE_API float fnGenSingle3D( const void* generator, float x, float y, float z, int seed );
FASTNOISE_API float fnGenSingle4D( const void* generator, float x, float y, float z, float w, int seed );

#ifdef __cplusplus
}
#endif

#endif
