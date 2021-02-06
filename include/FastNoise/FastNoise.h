#pragma once

#include "FastNoise_Config.h"

#include "Generators/BasicGenerators.h"
#include "Generators/Value.h"
#include "Generators/Perlin.h"
#include "Generators/Simplex.h"
#include "Generators/Cellular.h"
#include "Generators/Fractal.h"
#include "Generators/DomainWarp.h"
#include "Generators/DomainWarpFractal.h"
#include "Generators/Modifiers.h"
#include "Generators/Blends.h"

namespace FastNoise
{
    template<typename T>
    SmartNode<T> New( FastSIMD::eLevel maxLevel = FastSIMD::Level_Null )
    {
        static_assert( std::is_base_of<Generator, T>::value, "Use FastSIMD::New() to create non FastNoise classes" );

        return SmartNode<T>( FastSIMD::New<T>( maxLevel ) );
    }

    SmartNode<> NewFromEncodedNodeTree( const char* encodedNodeTreeString, FastSIMD::eLevel maxLevel = FastSIMD::Level_Null );
}