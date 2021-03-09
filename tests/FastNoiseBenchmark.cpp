#include <benchmark/benchmark.h>
#include "FastNoise/FastNoise.h"
#include "FastNoise/FastNoiseMetadata.h"

#include "../NoiseTool/DemoNodeTrees.inl"

#include "magic_enum.h"

FastNoise::SmartNode<> BuildGenerator( benchmark::State& state, const FastNoise::Metadata* metadata, FastSIMD::eLevel level )
{    
    FastNoise::SmartNode<> generator = metadata->CreateNode( level );

    FastNoise::SmartNode<> source = FastNoise::New<FastNoise::Constant>( level );

    for( const auto& memberNode : metadata->memberNodes )
    {
        if( !memberNode.setFunc( generator.get(), source ) )
        {
            // If constant source is not valid try all other node types in order
            for( const FastNoise::Metadata* tryMetadata : FastNoise::Metadata::GetMetadataClasses() )
            {
                FastNoise::SmartNode<> trySource = tryMetadata->CreateNode( level );

                // Other node types may also have sources
                if( memberNode.setFunc( generator.get(), trySource ) )
                {
                    for( const auto& tryMemberNode : tryMetadata->memberNodes )
                    {
                        if( !tryMemberNode.setFunc( trySource.get(), source ) )
                        {
                            state.SkipWithError( "Could not set valid sources for generator" );
                            return {};
                        }                        
                    }
                    break;
                }
            }
        }
    }
    return generator;
}

void BenchFastNoiseGenerator2D( benchmark::State& state, int32_t testSize, const FastNoise::SmartNode<> generator )
{
    if (!generator) return;

    size_t dataSize = (size_t)testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        generator->GenUniformGrid2D( data, 0, 0, testSize, testSize, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

void BenchFastNoiseGenerator3D( benchmark::State& state, int32_t testSize, const FastNoise::SmartNode<> generator )
{
    if (!generator) return;

    size_t dataSize = (size_t)testSize * testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        generator->GenUniformGrid3D( data, 0, 0, 0, testSize, testSize, testSize, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

void BenchFastNoiseGenerator4D( benchmark::State& state, int32_t testSize, const FastNoise::SmartNode<> generator )
{
    if (!generator) return;

    size_t dataSize = (size_t)testSize * testSize * testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        generator->GenUniformGrid4D( data, 0, 0, 0, 0, testSize, testSize, testSize, testSize, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

template<typename T>
void RegisterBenchmarks( FastSIMD::eLevel level, const char* groupName, const char* name, T generatorFunc )
{
    std::string benchName = "2D/";

#ifdef MAGIC_ENUM_SUPPORTED
    auto enumName = magic_enum::flags::enum_name( level );
    auto find = enumName.find( '_' );
    if( find != std::string::npos )
    {
        benchName += enumName.data() + find + 1;
    }
    else
    {
        benchName += enumName;
    }
#else
    benchName += std::to_string( (int)level );
#endif


    benchName += '/';
    benchName += groupName;
    benchName += '/';
    benchName += name;

    benchmark::RegisterBenchmark( benchName.c_str(), [=]( benchmark::State& st ) { BenchFastNoiseGenerator2D( st, 64, generatorFunc( st ) ); } );

    benchName[0] = '3';

    benchmark::RegisterBenchmark( benchName.c_str(), [=]( benchmark::State& st ) { BenchFastNoiseGenerator3D( st, 16, generatorFunc( st ) ); } );

    benchName[0] = '4';

    benchmark::RegisterBenchmark( benchName.c_str(), [=]( benchmark::State& st ) { BenchFastNoiseGenerator4D( st, 8, generatorFunc( st ) ); } );
}

int main( int argc, char** argv )
{
    benchmark::Initialize( &argc, argv );
    
    for( FastSIMD::eLevel level = FastSIMD::CPUMaxSIMDLevel(); level != FastSIMD::Level_Null; level = (FastSIMD::eLevel)(level >> 1) )
    {
        if( !(level & FastSIMD::COMPILED_SIMD_LEVELS & FastNoise::SUPPORTED_SIMD_LEVELS) )
        {
            continue;
        }

        for( const FastNoise::Metadata* metadata : FastNoise::Metadata::GetMetadataClasses() )
        {
            const char* groupName = "Misc";

            if( !metadata->groups.empty() )
            {
                groupName = metadata->groups[metadata->groups.size() - 1];
            }

            std::string nodeName = FastNoise::Metadata::FormatMetadataNodeName( metadata, false );

           RegisterBenchmarks( level, groupName, nodeName.c_str(), [=]( benchmark::State& st ) { return BuildGenerator( st, metadata, level ); } );
        }

        for( const auto& nodeTree : gDemoNodeTrees )
        {
            RegisterBenchmarks( level, "Node Trees", nodeTree[0], [=]( benchmark::State& st )
            {
                FastNoise::SmartNode<> rootNode = FastNoise::NewFromEncodedNodeTree( nodeTree[1], level );

                if( !rootNode )
                {
                    st.SkipWithError( "Could not generate node tree from encoded string" );                    
                }

                return rootNode;
            } );
            
        }
    }

    benchmark::RunSpecifiedBenchmarks();

    return 0;
}