#include <cstdio>
#include <string>

#include "FastNoise/CodeGen.h"

#include "CodeGenBenchmarkTrees.h"

// Writes C++ for every benchmark tree, compiled into a FastSIMD dispatch library by CodeGenBenchmarkNodes.inl
int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        std::fprintf( stderr, "Usage: %s <output file>\n", argv[0] );
        return 1;
    }

    std::string code = "#pragma once\n\n";

    for( int tree = 0; tree < kCodeGenBenchmarkTreeCount; tree++ )
    {
        for( int dimensions = 2; dimensions <= 4; dimensions++ )
        {
            FastNoise::CodeGen::Options options;
            options.dimensions = dimensions;
            options.functionName = "CodeGenTree" + std::to_string( tree ) + "_" + std::to_string( dimensions ) + "D";

            std::string generated = FastNoise::CodeGen::Generate( gCodeGenBenchmarkTrees[tree][1], FastNoise::CodeGen::Language::CPP, options );

            if( generated.empty() )
            {
                std::fprintf( stderr, "Failed to generate code for %s\n", gCodeGenBenchmarkTrees[tree][0] );
                return 1;
            }
            code += generated + "\n";
        }
    }

    code += "#define FASTNOISE_CODEGEN_BENCHMARK_TREES( X )";
    for( int tree = 0; tree < kCodeGenBenchmarkTreeCount; tree++ )
    {
        code += " X( " + std::to_string( tree ) + " )";
    }
    code += "\n";

    FILE* file = std::fopen( argv[1], "w" );
    if( !file )
    {
        return 1;
    }
    std::fputs( code.c_str(), file );
    std::fclose( file );
    return 0;
}
