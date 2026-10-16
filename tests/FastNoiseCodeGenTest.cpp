#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "FastNoise/FastNoise.h"
#include "FastNoise/Metadata.h"
#include "FastNoise/CodeGen.h"
#include "FastSIMD/FastSIMD_FastNoise_config.h"

#include "../tools/NodeEditor/util/DemoNodeTrees.inl"

// Checks traced programs reproduce the CPU SIMD output for every node type, the demo node trees and random node trees
// With FASTNOISE2_STRICT_FP the output must match the minimum feature set exactly
// Some AVX512 paths (4D gradients) sum in a different order, so even strict builds differ by an ulp between feature sets

#if FASTSIMD_IS_RELAXED
static constexpr bool kStrict = false;
#else
static constexpr bool kStrict = true;
#endif

using namespace FastNoise;

static constexpr int kSampleCount = 512;

// Optional JSON lines export of every case, for running the generated shaders on a GPU (see CodeGenGpuCheck.py)
static FILE* gExportFile = nullptr;

static void ExportString( const std::string& text )
{
    std::fputc( '"', gExportFile );
    for( char c : text )
    {
        switch( c )
        {
        case '"': std::fputs( "\\\"", gExportFile ); break;
        case '\\': std::fputs( "\\\\", gExportFile ); break;
        case '\n': std::fputs( "\\n", gExportFile ); break;
        default: std::fputc( c, gExportFile ); break;
        }
    }
    std::fputc( '"', gExportFile );
}

static void ExportFloats( const float* values, size_t count )
{
    std::fputc( '[', gExportFile );
    for( size_t i = 0; i < count; i++ )
    {
        if( std::isfinite( values[i] ) )
        {
            std::fprintf( gExportFile, i ? ",%.9g" : "%.9g", values[i] );
        }
        else
        {
            std::fputs( i ? ",null" : "null", gExportFile );
        }
    }
    std::fputc( ']', gExportFile );
}

static void ExportCase( const std::string& name, const char* encoded, int dimensions, int seed, const std::vector<float>* positions,
    const std::vector<float>& reference, const std::vector<float>& cpu, const CodeGen::Program& program )
{
    std::vector<float> interleaved;
    for( int i = 0; i < kSampleCount; i++ )
    {
        for( int d = 0; d < dimensions; d++ )
        {
            interleaved.push_back( positions[d][i] );
        }
    }

    std::fputs( "{\"name\":", gExportFile );
    ExportString( name );
    std::fprintf( gExportFile, ",\"encoded\":" );
    ExportString( encoded );
    std::fprintf( gExportFile, ",\"dimensions\":%d,\"seed\":%d,\"positions\":", dimensions, seed );
    ExportFloats( interleaved.data(), interleaved.size() );
    std::fputs( ",\"reference\":", gExportFile );
    ExportFloats( reference.data(), reference.size() );
    std::fputs( ",\"cpu\":", gExportFile );
    ExportFloats( cpu.data(), cpu.size() );
    std::fputs( ",\"glsl\":", gExportFile );
    ExportString( program.Emit( CodeGen::Language::GLSL ) );
    std::fputs( ",\"hlsl\":", gExportFile );
    ExportString( program.Emit( CodeGen::Language::HLSL ) );
    std::fputs( ",\"wgsl\":", gExportFile );
    ExportString( program.Emit( CodeGen::Language::WGSL ) );
    std::fputs( "}\n", gExportFile );
}

struct Stats
{
    int cases = 0;
    int failures = 0;
    int chaotic = 0;
    int nonDeterministic = 0;
};

// +0 and -0 are considered equal, SSE2 and SSE4.1 FS::Round already disagree on the sign of zero
static bool SameFloat( float a, float b )
{
    return a == b || ( std::isnan( a ) && std::isnan( b ) );
}

struct Comparison
{
    int exact = 0;
    int close = 0;
    double maxError = 0;
};

// Fast math simplifications (x * 0 -> 0) are allowed to differ where the reference is NaN or inf
static Comparison Compare( const std::vector<float>& a, const std::vector<float>& b, bool fastMath = false )
{
    Comparison result;
    float range = 1e-6f;

    for( float v : b )
    {
        if( std::isfinite( v ) )
        {
            range = std::max( range, std::abs( v ) );
        }
    }

    for( size_t i = 0; i < a.size(); i++ )
    {
        float error = std::abs( a[i] - b[i] );
        bool same = SameFloat( a[i], b[i] );

        result.exact += same;
        result.close += same || error <= 1e-3f * range || ( fastMath && !std::isfinite( b[i] ) );
        if( std::isfinite( error ) )
        {
            result.maxError = std::max<double>( result.maxError, error );
        }
    }
    return result;
}

static void GenerateCpu( const SmartNode<>& generator, int dimensions, const std::vector<float>* positions, int seed, std::vector<float>& out )
{
    out.resize( kSampleCount );

    switch( dimensions )
    {
    case 2: generator->GenPositionArray2D( out.data(), kSampleCount, positions[0].data(), positions[1].data(), 0, 0, seed ); break;
    case 3: generator->GenPositionArray3D( out.data(), kSampleCount, positions[0].data(), positions[1].data(), positions[2].data(), 0, 0, 0, seed ); break;
    case 4: generator->GenPositionArray4D( out.data(), kSampleCount, positions[0].data(), positions[1].data(), positions[2].data(), positions[3].data(), 0, 0, 0, 0, seed ); break;
    }
}

static bool CompareTree( const std::string& name, const char* encoded, int dimensions, std::mt19937& rng, Stats& stats )
{
    stats.cases++;

    // Generic code paths, which tracing also takes
    SmartNode<> generatorMin = NewFromEncodedNodeTree( encoded, FastSIMD::FastSIMD_FastNoise::CompiledFeatureSets::Minimum );
    SmartNode<> generatorMax = NewFromEncodedNodeTree( encoded );

    CodeGen::Options options;
    options.dimensions = dimensions;
    options.fastMath = !kStrict;

    std::string error;
    auto program = CodeGen::Program::FromEncodedNodeTree( encoded, options, &error );

    if( !generatorMin || !program )
    {
        std::printf( "FAIL %s %dD: %s\n", name.c_str(), dimensions, generatorMin ? error.c_str() : "could not create generator" );
        stats.failures++;
        return false;
    }

    std::uniform_real_distribution<float> posDist( -1000.0f, 1000.0f );
    std::vector<float> positions[4];
    int seed = (int)rng();

    for( auto& axis : positions )
    {
        axis.resize( kSampleCount );
        for( float& pos : axis )
        {
            pos = posDist( rng );
        }
    }

    for( CodeGen::Language language : { CodeGen::Language::GLSL, CodeGen::Language::HLSL, CodeGen::Language::WGSL, CodeGen::Language::CPP } )
    {
        if( program->Emit( language ).empty() )
        {
            std::printf( "FAIL %s %dD: empty code emitted\n", name.c_str(), dimensions );
            stats.failures++;
            return false;
        }
    }

    std::vector<float> cpuMin, cpuMax, traced( kSampleCount );
    GenerateCpu( generatorMin, dimensions, positions, seed, cpuMin );
    GenerateCpu( generatorMax, dimensions, positions, seed, cpuMax );

    for( int i = 0; i < kSampleCount; i++ )
    {
        float pos[4] = { positions[0][i], positions[1][i], positions[2][i], positions[3][i] };
        traced[i] = program->Evaluate( pos, seed );
    }

    if( gExportFile )
    {
        ExportCase( name, encoded, dimensions, seed, positions, traced, cpuMax, *program );
    }

    Comparison toMin = Compare( traced, cpuMin, options.fastMath );
    Comparison toMax = Compare( traced, cpuMax, options.fastMath );
    Comparison cpuToCpu = Compare( cpuMax, cpuMin );
    bool pass;

    if( kStrict )
    {
        // Strict feature sets can still disagree with each other: AVX512 4D gradients sum in a different order, and CellularValue/CellularLookup
        // read uninitialised registers when every candidate distance is inf/NaN (Minkowski P near 0), giving a different value in every build
        int mismatches = 0;
        int nonDeterministic = 0;

        for( int i = 0; i < kSampleCount; i++ )
        {
            if( !SameFloat( traced[i], cpuMin[i] ) && !SameFloat( traced[i], cpuMax[i] ) )
            {
                ( SameFloat( cpuMin[i], cpuMax[i] ) ? mismatches : nonDeterministic )++;
            }
        }

        pass = mismatches == 0;
        stats.nonDeterministic += nonDeterministic != 0;
    }
    else
    {
        // Relaxed feature sets use FMA and rsqrt/rcp approximations, so they already disagree with each other.
        // Discontinuous nodes amplify that near edges, tracing (exact math) should be about as close to the CPU as the CPU feature sets are to each other
        int close = std::max( toMin.close, toMax.close );
        pass = close >= std::min( kSampleCount * 98 / 100, cpuToCpu.close * 9 / 10 ) - kSampleCount / 100;

        // Some random trees hash positions (White noise), relaxed math makes them disagree almost everywhere
        if( !pass && cpuToCpu.close < kSampleCount / 2 )
        {
            stats.chaotic++;
            return true;
        }
    }

    if( !pass )
    {
        std::printf( "FAIL %s %dD: exact %d/%d close %d/%d max error %g (cpu feature sets close %d/%d) %s\n", name.c_str(), dimensions,
            toMin.exact, kSampleCount, std::max( toMin.close, toMax.close ), kSampleCount, toMin.maxError, cpuToCpu.close, kSampleCount, encoded );
        stats.failures++;
    }
    return pass;
}

// Small tree with varying output used to drive node sources
static NodeData* NewSourceTree( std::vector<std::unique_ptr<NodeData>>& storage, std::mt19937& rng )
{
    static const Metadata* kSourceTypes[] = { &Metadata::Get<Simplex>(), &Metadata::Get<Perlin>(), &Metadata::Get<Value>(), &Metadata::Get<SuperSimplex>() };

    NodeData* node = storage.emplace_back( new NodeData( kSourceTypes[rng() % 4] ) ).get();
    // Feature scale
    node->variables[0] = std::uniform_real_distribution<float>( 20.0f, 300.0f )( rng );
    // Seed offset
    node->variables[1] = (int)( rng() % 100 );
    return node;
}

static void RandomiseVariables( NodeData* node, std::mt19937& rng )
{
    for( size_t i = 0; i < node->variables.size(); i++ )
    {
        const auto& member = node->metadata->memberVariables[i];

        switch( member.type )
        {
        case Metadata::MemberVariable::EFloat:
        {
            // Metadata min/max are 0 when unbounded, max is 0 when only a min is given
            float def = member.valueDefault.f;
            float spread = std::max( 1.0f, std::abs( def ) );
            float min = member.valueMin.f, max = member.valueMax.f;
            if( min == max )
            {
                min = def - spread;
            }
            if( max <= min )
            {
                max = std::max( min, def ) + spread;
            }
            node->variables[i] = std::uniform_real_distribution<float>( min, max )( rng );
            break;
        }
        case Metadata::MemberVariable::EInt:
        {
            int def = member.valueDefault.i;
            int min = member.valueMin.i, max = member.valueMax.i;
            if( min == max )
            {
                min = def - 3;
            }
            if( max <= min )
            {
                max = std::max( min, def ) + 3;
            }
            node->variables[i] = std::uniform_int_distribution<int>( min, max )( rng );
            break;
        }
        case Metadata::MemberVariable::EEnum:
            node->variables[i] = (int)( rng() % member.enumNames.size() );
            break;
        }
    }
}

// Picks a random metadata that can be used as the source of the given node lookup
static const Metadata* FindValidSource( const Metadata* metadata, size_t lookupIdx, std::mt19937& rng, bool preferLeaf )
{
    SmartNode<> test = metadata->CreateNode();
    std::vector<const Metadata*> candidates( Metadata::GetAll().begin(), Metadata::GetAll().end() );
    std::shuffle( candidates.begin(), candidates.end(), rng );

    if( preferLeaf )
    {
        std::stable_partition( candidates.begin(), candidates.end(), []( const Metadata* m ) { return m->memberNodeLookups.size() == 0; } );
    }

    for( const Metadata* candidate : candidates )
    {
        if( metadata->memberNodeLookups[lookupIdx].setFunc( test.get(), candidate->CreateNode() ) )
        {
            return candidate;
        }
    }
    return nullptr;
}

static NodeData* NewRandomTree( std::vector<std::unique_ptr<NodeData>>& storage, std::mt19937& rng, int depth, const Metadata* metadata = nullptr )
{
    const auto& all = Metadata::GetAll();

    if( !metadata )
    {
        metadata = all[rng() % all.size()];
    }

    NodeData* node = storage.emplace_back( new NodeData( metadata ) ).get();
    RandomiseVariables( node, rng );

    // Keep fractals and cellular cheap enough to evaluate
    for( size_t i = 0; i < node->variables.size(); i++ )
    {
        if( std::strcmp( metadata->memberVariables[i].name, "Octaves" ) == 0 )
        {
            node->variables[i] = 2 + (int)( rng() % 3 );
        }
    }

    for( size_t i = 0; i < node->nodeLookups.size(); i++ )
    {
        const Metadata* sourceType = depth > 0 ? FindValidSource( metadata, i, rng, depth <= 1 ) : nullptr;

        if( sourceType && !sourceType->memberNodeLookups.size() && rng() % 2 )
        {
            node->nodeLookups[i] = NewSourceTree( storage, rng );
        }
        else
        {
            node->nodeLookups[i] = sourceType ? NewRandomTree( storage, rng, depth - 1, sourceType ) : nullptr;
        }

        // Fallback to sources that are always valid when random selection failed
        if( !node->nodeLookups[i] )
        {
            const Metadata* fallback = FindValidSource( metadata, i, rng, true );
            node->nodeLookups[i] = fallback ? NewRandomTree( storage, rng, 0, fallback ) : nullptr;
        }
    }

    for( auto& hybrid : node->hybrids )
    {
        if( depth > 0 && rng() % 3 == 0 )
        {
            hybrid.first = NewSourceTree( storage, rng );
        }
        else
        {
            hybrid.second = std::uniform_real_distribution<float>( -2.0f, 2.0f )( rng );
        }
    }
    return node;
}

int main( int argc, char** argv )
{
    int randomTreeCount = 300;

    for( int i = 1; i < argc; i++ )
    {
        if( std::strcmp( argv[i], "--export" ) == 0 && i + 1 < argc )
        {
            gExportFile = std::fopen( argv[++i], "w" );
        }
        else
        {
            randomTreeCount = std::atoi( argv[i] );
        }
    }

    std::printf( "FastNoise2 CodeGen validation (%s floating point)\n", kStrict ? "strict" : "relaxed" );

    std::mt19937 rng( 1337 );
    Stats stats;

    // Every node type, with sources driven by coherent noise so they don't degenerate to constants
    for( const Metadata* metadata : Metadata::GetAll() )
    {
        for( int variant = 0; variant < 4; variant++ )
        {
            std::vector<std::unique_ptr<NodeData>> storage;
            NodeData* node = storage.emplace_back( new NodeData( metadata ) ).get();

            if( variant )
            {
                RandomiseVariables( node, rng );
            }

            for( size_t i = 0; i < node->nodeLookups.size(); i++ )
            {
                const Metadata* sourceType = FindValidSource( metadata, i, rng, true );
                node->nodeLookups[i] = sourceType == &Metadata::Get<Constant>() || variant == 0 ? NewSourceTree( storage, rng ) : NewRandomTree( storage, rng, 0, sourceType );

                SmartNode<> test = metadata->CreateNode();
                if( !metadata->memberNodeLookups[i].setFunc( test.get(), node->nodeLookups[i]->metadata->CreateNode() ) )
                {
                    node->nodeLookups[i] = NewRandomTree( storage, rng, 0, sourceType );
                }
            }
            for( auto& hybrid : node->hybrids )
            {
                if( variant >= 2 )
                {
                    hybrid.first = NewSourceTree( storage, rng );
                }
            }

            std::string encoded = Metadata::SerialiseNodeData( node );
            std::string name = std::string( metadata->name ) + " #" + std::to_string( variant );

            for( int dimensions = 2; dimensions <= 4; dimensions++ )
            {
                CompareTree( name, encoded.c_str(), dimensions, rng, stats );
            }
        }
    }

    for( const auto& nodeTree : gDemoNodeTrees )
    {
        for( int dimensions = 2; dimensions <= 4; dimensions++ )
        {
            CompareTree( nodeTree[0], nodeTree[1], dimensions, rng, stats );
        }
    }

    for( int i = 0; i < randomTreeCount; i++ )
    {
        std::vector<std::unique_ptr<NodeData>> storage;
        NodeData* root = NewRandomTree( storage, rng, 3 );
        std::string encoded = Metadata::SerialiseNodeData( root );

        if( encoded.empty() )
        {
            continue;
        }
        CompareTree( "Random #" + std::to_string( i ), encoded.c_str(), 2 + i % 3, rng, stats );
    }

    std::printf( "%d/%d cases passed", stats.cases - stats.failures, stats.cases );
    if( stats.chaotic )
    {
        std::printf( " (%d not comparable, relaxed CPU feature sets disagree on most samples)", stats.chaotic );
    }
    if( stats.nonDeterministic )
    {
        std::printf( " (%d with samples where the strict CPU feature sets and traced output all disagree, not compared)", stats.nonDeterministic );
    }
    std::printf( "\n" );

    if( gExportFile )
    {
        std::fclose( gExportFile );
    }
    return stats.failures ? 1 : 0;
}
