#pragma once

#include <memory>

#include "FastSIMD/FastSIMD.h"
#include "FastNoise_Config.h"

#include "Generators/BasicGenerators.h"
#include "Generators/White.h"
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
    inline std::shared_ptr<T> New()
    {
        static_assert( std::is_base_of_v<Generator, T>, "Use FastSIMD::New() to create non FastNoise classes" );

        return std::shared_ptr<T>( FastSIMD::New<T>() );
    }

    template<typename T>
    inline std::shared_ptr<T> New( FastSIMD::eLevel maxLevel )
    {
        static_assert( std::is_base_of_v<Generator, T>, "Use FastSIMD::New() to create non FastNoise classes" );

        return std::shared_ptr<T>( FastSIMD::New<T>( maxLevel ) );
    }
}