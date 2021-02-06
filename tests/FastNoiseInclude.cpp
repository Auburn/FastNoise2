#include <FastNoise/FastNoise.h>
//#include <FastNoise/FastNoiseMetadata.h>

#include <iostream>

int main()
{
    FastNoise::SmartNode<> node = FastNoise::New<FastNoise::Simplex>();

    float noise[16 * 16];

    node->GenUniformGrid2D( noise, 0, 0, 16, 16, 0.02f, 1337 );

    for( int i = 0; i < sizeof(noise) / sizeof(float); i++ )
    {
        std::cout << noise[i] << std::endl;
    }
}