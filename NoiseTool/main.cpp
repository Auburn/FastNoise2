#include "FastNoise/FastNoise.h"
#include <iostream>

int main()
{    
    auto generator = FastNoise::New<FastNoise::FractalFBm>();

    generator->SetSource( FastNoise::New<FastNoise::Simplex>() );

    std::cout << "SIMD Level:  " << generator->GetSIMDLevel() << "\n";

    float noise[8 * 8 * 8];

    generator->GenUniformGrid3D( noise, 0, 0, 0, 8, 8, 8, 0.1f, 0.1f, 0.1f, 1337 );

    for ( int i = 0; i < 8 * 8 * 8; i++ )
    {
        std::cout << noise[i] << ";";
    }

    getchar();
}