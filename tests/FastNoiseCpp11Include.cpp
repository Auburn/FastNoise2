#include <FastNoise/FastNoise.h>
//#include <FastNoise/FastNoiseMetadata.h>

#include <iostream>

int main()
{
    auto node = FastNoise::New<FastNoise::FractalFBm>();

    node->SetSource( FastNoise::New<FastNoise::Simplex>() );
    node->SetGain( FastNoise::New<FastNoise::Value>() );

    {
        FastNoise::SmartNode<> base = FastNoise::New<FastNoise::Simplex>();

        FastNoise::SmartNode<FastNoise::Simplex> simplex = FastNoise::SmartNode<FastNoise::Simplex>::DynamicCast( base );
    }

    const int size = 16;

    float noise[size * size];

    node->GenUniformGrid2D( noise, 0, 0, size, size, 0.02f, 1337 );

    for( int i = 0; i < sizeof(noise) / sizeof(float); i++ )
    {
        std::cout << noise[i] << std::endl;
    }
}