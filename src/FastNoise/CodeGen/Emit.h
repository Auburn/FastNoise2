#pragma once
#include <string>

#include <FastNoise/CodeGen.h>

#include "IR.h"

namespace FastNoise
{
    namespace CodeGen
    {
        std::string EmitProgram( const IR::Program& program, Language language, const std::string& functionName );
    }
}
