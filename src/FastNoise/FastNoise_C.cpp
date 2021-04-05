#include <FastNoise/FastNoise_C.h>
#include <FastNoise/FastNoise.h>

FastNoise::Generator* ToGen( void* p )
{
    return static_cast<FastNoise::SmartNode<>*>( p )->get();
}

FastNoise::Generator* ToGen( const void* p )
{
    return static_cast<const FastNoise::SmartNode<>*>( p )->get();
}

void StoreMinMax( float* floatArray2, FastNoise::OutputMinMax minMax )
{
    if( floatArray2 )
    {
        floatArray2[0] = minMax.min;
        floatArray2[1] = minMax.max;
    }
}

void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned simdLevel )
{
    return new FastNoise::SmartNode<>( FastNoise::NewFromEncodedNodeTree( encodedString, (FastSIMD::eLevel)simdLevel ) );
}

void fnDeleteGeneratorRef( void* generator )
{
    delete static_cast<FastNoise::SmartNode<>*>( generator );
}

unsigned fnGetSIMDLevel( const void* generator )
{
    return ToGen( generator )->GetSIMDLevel();
}

void fnGenUniformGrid2D( const void* generator, float* noiseOut, int xStart, int yStart, int xSize, int ySize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenUniformGrid2D( noiseOut, xStart, yStart, xSize, ySize, frequency, seed ) );    
}

void fnGenUniformGrid3D( const void* generator, float* noiseOut, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenUniformGrid3D( noiseOut, xStart, yStart, zStart, xSize, ySize, zSize, frequency, seed ) );    
}

void fnGenUniformGrid4D( const void* generator, float* noiseOut, int xStart, int yStart, int zStart, int wStart, int xSize, int ySize, int zSize, int wSize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenUniformGrid4D( noiseOut, xStart, yStart, zStart, wStart, xSize, ySize, zSize, wSize, frequency, seed ) );    
}

void fnGenPositionArray2D( const void* generator, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenPositionArray2D( noiseOut, count, xPosArray, yPosArray, xOffset, yOffset, seed ) );
}

void fnGenPositionArray3D( const void* generator, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenPositionArray3D( noiseOut, count, xPosArray, yPosArray, zPosArray, xOffset, yOffset, zOffset, seed ) );
}

void fnGenPositionArray4D( const void* generator, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenPositionArray4D( noiseOut, count, xPosArray, yPosArray, zPosArray, wPosArray, xOffset, yOffset, zOffset, wOffset, seed ) );
}

float fnGenSingle2D( const void* generator, float x, float y, int seed )
{
    return ToGen( generator )->GenSingle2D( x, y, seed );
}

float fnGenSingle3D( const void* generator, float x, float y, float z, int seed )
{
    return ToGen( generator )->GenSingle3D( x, y, z, seed );
}

float fnGenSingle4D( const void* generator, float x, float y, float z, float w, int seed )
{
    return ToGen( generator )->GenSingle4D( x, y, z, w, seed );
}

void fnGenTileable2D( const void* generator, float* noiseOut, int xSize, int ySize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( generator )->GenTileable2D( noiseOut, xSize, ySize, frequency, seed ) );
}
