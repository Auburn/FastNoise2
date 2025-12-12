#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>

#include <iostream>

int main()
{
    auto node = FastNoise::New<FastNoise::FractalFBm>();

    std::cout << (unsigned)node->GetActiveFeatureSet() << std::endl;

    node->SetSource( FastNoise::New<FastNoise::Simplex>() );
    node->SetGain( FastNoise::New<FastNoise::Value>() );

    const int size = 4;

    float noise[size * size];

    node->GenUniformGrid2D( noise, 0.0f, 0.0f, size, size, 1.0f, 1.0f, 1337 );

    for( int i = 0; i < sizeof(noise) / sizeof(float); i++ )
    {
        std::cout << noise[i] << ", ";
    }

    std::cout << std::endl;

    // SmartNode down cast example
    {
        // New Checkerboard node stored in base SmartNode type
        FastNoise::SmartNode<> base = FastNoise::New<FastNoise::Checkerboard>();

        // Compile error
        // base->SetSize( 8.0f );

        // Down cast to known type
        auto checkerboard = FastNoise::SmartNode<FastNoise::Checkerboard>::DynamicCast( base );

        // Ok
        checkerboard->SetScale( 8.0f );

        // Down cast to wrong type will return nullptr
        auto simplex = FastNoise::SmartNode<FastNoise::Simplex>::DynamicCast( base );

        std::cout << ( simplex ? "valid" : "nullptr" ) << std::endl;
    }
}