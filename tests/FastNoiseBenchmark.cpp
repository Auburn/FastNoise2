#include <benchmark/benchmark.h>

#include "FastNoise/FastNoise.h"
#include "FastSIMD/TypeList.h"


void BenchFastNoiseGenerator( benchmark::State& state, int testSize, const FastNoise::Metadata* metadata, FastSIMD::eLevel level )
{
    std::shared_ptr<FastNoise::Generator> generator( metadata->NodeFactory( level ) );

    std::shared_ptr<FastNoise::Generator> source( FastSIMD::New<FastNoise::Constant>( level ) );

    for( int i = 0; i < metadata->memberNodes.size(); i++ )
    {
        if( !metadata->memberNodes[i].setFunc( generator.get(), source ) )
        {
            state.SkipWithError( "Could not set valid sources for generator" );
            return;
        }
    }

    size_t dataSize = (size_t)testSize * testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        generator->GenUniformGrid3D( data, 0, 0, 0, testSize, testSize, testSize, 0.1f, 0.1f, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

int main( int argc, char** argv )
{
    benchmark::Initialize( &argc, argv );
    
    std::string benchName;

    for( FastSIMD::eLevel level = FastSIMD::CPUMaxSIMDLevel(); level != FastSIMD::Level_Null; level = (FastSIMD::eLevel)(level >> 1) )
    {
        if( !(level & FastSIMD::COMPILED_SIMD_LEVELS) )
        {
            continue;
        }

        for( const FastNoise::Metadata* metadata : FastNoise::MetadataManager::GetMetadataClasses() )
        {
            benchName = metadata->name;
            benchName += '/';
            benchName += std::to_string( level );

            benchmark::RegisterBenchmark( benchName.c_str(), BenchFastNoiseGenerator, 32, metadata, level );
        }
    }

    benchmark::RunSpecifiedBenchmarks();

    getchar();
    return 0;
}