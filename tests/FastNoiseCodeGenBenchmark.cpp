#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include "FastNoise/FastNoise.h"
#include "FastNoise/Metadata.h"
#include "FastSIMD/FastSIMD_FastNoise_config.h"

#include "CodeGenBenchmarkNodes.h"
#include "CodeGenBenchmarkTrees.h"

// Runtime node trees (a virtual call per node per SIMD vector) against the same trees compiled to C++ by FastNoise::CodeGen

static const size_t gPositionCount = 8192;
static float gPositionFloats[gPositionCount];

const FastNoise::Metadata& FastNoise::CodeGenBenchmarkMetadata()
{
    return Metadata::Get<Constant>();
}

using GeneratedFactory = FastNoise::SmartNode<> ( * )( FastSIMD::FeatureSet );

template<int... TREE>
static std::array<GeneratedFactory, sizeof...( TREE )> MakeGeneratedFactories( std::integer_sequence<int, TREE...> )
{
    return { { []( FastSIMD::FeatureSet level ) -> FastNoise::SmartNode<> { return FastNoise::New<FastNoise::CodeGenBenchmarkTree<TREE>>( level ); }... } };
}

static const auto gGeneratedFactories = MakeGeneratedFactories( std::make_integer_sequence<int, kCodeGenBenchmarkTreeCount>() );

static void Generate( const FastNoise::SmartNode<>& generator, int dimensions, float* out, int seed )
{
    switch( dimensions )
    {
    case 2: generator->GenPositionArray2D( out, gPositionCount, gPositionFloats, gPositionFloats, 0, 0, seed ); break;
    case 3: generator->GenPositionArray3D( out, gPositionCount, gPositionFloats, gPositionFloats, gPositionFloats, 0, 0, 0, seed ); break;
    case 4: generator->GenPositionArray4D( out, gPositionCount, gPositionFloats, gPositionFloats, gPositionFloats, gPositionFloats, 0, 0, 0, 0, seed ); break;
    }
}

static void BenchGenerator( benchmark::State& state, const FastNoise::SmartNode<>& generator, int dimensions )
{
    if( !generator )
    {
        state.SkipWithError( "Could not create generator" );
        return;
    }

    std::vector<float> data( gPositionCount );
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        Generate( generator, dimensions, data.data(), seed++ );
        totalData += gPositionCount;
    }

    state.SetItemsProcessed( totalData );
}

// Generated code is traced from the same node implementations, report how closely it matches
static void PrintAgreement( const char* name, int dimensions, FastSIMD::FeatureSet level, const FastNoise::SmartNode<>& runtime, const FastNoise::SmartNode<>& generated )
{
    std::vector<float> a( gPositionCount ), b( gPositionCount );
    Generate( runtime, dimensions, a.data(), 1337 );
    Generate( generated, dimensions, b.data(), 1337 );

    size_t exact = 0;
    double maxError = 0;
    for( size_t i = 0; i < gPositionCount; i++ )
    {
        exact += a[i] == b[i];
        maxError = std::max<double>( maxError, std::abs( a[i] - b[i] ) );
    }

    std::printf( "%dD %-7s %-18s exact %5.1f%%  max error %g\n", dimensions, FastSIMD::GetFeatureSetString( level ), name, 100.0 * exact / gPositionCount, maxError );
}

int main( int argc, char** argv )
{
    std::printf( "FastSIMD Max Supported Feature Set: %s\n", FastSIMD::GetFeatureSetString( FastSIMD::DetectCpuMaxFeatureSet() ) );

    benchmark::Initialize( &argc, argv );

    for( size_t idx = 0; idx < gPositionCount; idx++ )
    {
        gPositionFloats[idx] = (float)idx * 0.6f;
    }

    for( auto level : FastSIMD::FastSIMD_FastNoise::CompiledFeatureSets::AsArray )
    {
        if( level > FastSIMD::DetectCpuMaxFeatureSet() )
        {
            continue;
        }

        for( int tree = 0; tree < kCodeGenBenchmarkTreeCount; tree++ )
        {
            const char* name = gCodeGenBenchmarkTrees[tree][0];
            FastNoise::SmartNode<> runtime = FastNoise::NewFromEncodedNodeTree( gCodeGenBenchmarkTrees[tree][1], level );
            FastNoise::SmartNode<> generated = gGeneratedFactories[tree]( level );

            for( int dimensions = 2; dimensions <= 4; dimensions++ )
            {
                std::string benchName = std::to_string( dimensions ) + "D/" + FastSIMD::GetFeatureSetString( level ) + "/" + name;

                if( runtime && generated )
                {
                    PrintAgreement( name, dimensions, level, runtime, generated );
                }

                benchmark::RegisterBenchmark( ( benchName + "/Runtime" ).c_str(), [=]( benchmark::State& st ) { BenchGenerator( st, runtime, dimensions ); } );
                benchmark::RegisterBenchmark( ( benchName + "/CodeGen" ).c_str(), [=]( benchmark::State& st ) { BenchGenerator( st, generated, dimensions ); } );
            }
        }
    }

    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
