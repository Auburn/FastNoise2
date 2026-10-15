// Prints shader or C++ source for an encoded node tree.
//
//   NoiseCodegen <encoded node tree> [glsl|hlsl|wgsl|cpp] [2|3|4] [entry name]

#include <FastNoise/Codegen.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        std::fprintf( stderr, "usage: NoiseCodegen <encoded node tree> [glsl|hlsl|wgsl|cpp] [2|3|4] [entry name]\n" );
        return 1;
    }

    FastNoise::Codegen::Options options;

    if( argc > 2 )
    {
        if( !std::strcmp( argv[2], "hlsl" ) ) options.language = FastNoise::Codegen::Language::HLSL;
        else if( !std::strcmp( argv[2], "wgsl" ) ) options.language = FastNoise::Codegen::Language::WGSL;
        else if( !std::strcmp( argv[2], "cpp" ) ) options.language = FastNoise::Codegen::Language::CppScalar;
        else if( std::strcmp( argv[2], "glsl" ) ) { std::fprintf( stderr, "unknown language: %s\n", argv[2] ); return 1; }
    }
    if( argc > 3 )
    {
        options.dimensions = std::atoi( argv[3] );
    }
    if( argc > 4 )
    {
        options.entryPoint = argv[4];
    }

    std::string source = FastNoise::Codegen::Generate( argv[1], options );

    if( source.empty() )
    {
        std::fprintf( stderr, "Failed to decode node tree or unsupported dimension count\n" );
        return 1;
    }

    std::fputs( source.c_str(), stdout );
    return 0;
}
