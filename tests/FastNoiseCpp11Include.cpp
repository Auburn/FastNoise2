#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>

#include <taskflow/taskflow.hpp>

#include <array>
#include <iostream>

int main()
{
#if 0

    auto node = FastNoise::New<FastNoise::FractalFBm>();

    std::cout << node->GetSIMDLevel() << std::endl;

    node->SetSource( FastNoise::New<FastNoise::Simplex>() );
    node->SetGain( FastNoise::New<FastNoise::Value>() );

    const int size = 4;
    float noise[size * size];

    node->GenUniformGrid2D( noise, 0, 0, size, size, 0.02f, 1337 );

    for( int i = 0; i < sizeof(noise) / sizeof(float); i++ )
    {
        std::cout << noise[i] << ", ";
    }

    std::cout << std::endl;

#else

    const int size = 16;

    // Multi-threaded use-case
    std::array<std::array<float, size * size>, 16U> noise_per_thread;
    tf::Taskflow task_flow;
    task_flow.for_each_index(0U, 64U, 1U,
                              [&noise_per_thread, size]( const uint32_t t ) {
                                auto node = FastNoise::New<FastNoise::FractalFBm>();
                                node->SetSource( FastNoise::New<FastNoise::Simplex>() );
                                node->SetGain( FastNoise::New<FastNoise::Value>() );
                                node->GenUniformGrid2D( noise_per_thread[t].data(), 0, 0, size, size, 0.02f, 1337 );
                            });
    tf::Executor().run( task_flow ).get();

    for( int t = 0; t < noise_per_thread.size(); ++t )
    {
        std::cout << std::endl << "Thread " << t << " noise: ";
        for( float value: noise_per_thread[t] )
        {
            std::cout << value << ", ";
        }
    }
        

#endif

    // SmartNode down cast example
#if !FASTNOISE_USE_SHARED_PTR
    {
        // New Checkerboard node stored in base SmartNode type
        FastNoise::SmartNode<> base = FastNoise::New<FastNoise::Checkerboard>();

        // Compile error
        // base->SetSize( 8.0f );

        // Down cast to known type
        auto checkerboard = FastNoise::SmartNode<FastNoise::Checkerboard>::DynamicCast( base );

        // Ok
        checkerboard->SetSize( 8.0f );

        // Down cast to wrong type will return nullptr
        auto simplex = FastNoise::SmartNode<FastNoise::Simplex>::DynamicCast( base );

        std::cout << ( simplex ? "valid" : "nullptr" ) << std::endl;
    }
#endif
}