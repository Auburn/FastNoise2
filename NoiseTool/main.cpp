#include "../FastNoise/FastNoise.h"
#include <iostream>

int main()
{
    FastNoise::Simplex* simplex = FastSIMD::NewSIMDClass<FastNoise::Simplex>();

    float noise[8 * 8 * 8];

    simplex->GenUniformGrid3D( noise, 0, 0, 0, 8, 8, 8, 1.0f, 1.0f, 1.0f, 1337 );

    for ( int i = 0; i < 8 * 8 * 8; i++ )
    {
        std::cout << noise[i] << ";";
    }

    getchar();
}