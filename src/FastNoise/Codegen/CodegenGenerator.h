#pragma once
// Replacement for FastSIMD::DispatchClass<Generator, SIMD> (Generator.inl) when the node
// code is instantiated for the symbolic feature set. Source lookups become Call
// instructions and the batch generation API is stubbed out.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>

#include <FastNoise/Generators/Generator.h>

#include "CodegenRegister.h"

namespace FastSIMD
{
    template<typename T, FastSIMD::FeatureSet SIMD>
    class DispatchClass;
}

namespace FastNoise::Codegen
{
    // Must match kRegisterSize in Generator.inl for a feature set without any SIMD flags.
    // If the formula there changes, the Gen overrides in node code stop matching the base
    // class signatures below and the build fails loudly rather than silently diverging.
    static constexpr std::size_t kRegisterSize = std::max<std::size_t>( 4, FS::NativeRegisterCount<float>( kFeatureSet ) * 2 );

    using float32v = FS::Register<float, kRegisterSize, kFeatureSet>;
    using int32v = FS::Register<std::int32_t, kRegisterSize, kFeatureSet>;
    using mask32v = float32v::MaskType;

    // Implemented in Codegen.cpp: hands out function indices for (node, dimension count)
    class Tracer
    {
    public:
        virtual ~Tracer() = default;
        virtual std::int32_t RequestFunction( const Generator* node, int dimensions ) = 0;
    };
}

template<>
class FastSIMD::DispatchClass<FastNoise::Generator, FastNoise::Codegen::kFeatureSet> : public virtual FastNoise::Generator
{
    using float32v = FastNoise::Codegen::float32v;
    using int32v = FastNoise::Codegen::int32v;

public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const { return Gen( seed, x, y, z ); }

    FastSIMD::FeatureSet GetActiveFeatureSet() const final
    {
        return FastNoise::Codegen::kFeatureSet;
    }

    using VoidPtrStorageType = const DispatchClass<FastNoise::Generator, FastNoise::Codegen::kFeatureSet>*;

    void SetSourceSIMDPtr( const FastNoise::Generator* base, const void** simdPtr ) final
    {
        if( !base )
        {
            *simdPtr = nullptr;
            return;
        }
        auto simd = dynamic_cast<VoidPtrStorageType>( base );

        assert( simd );
        *simdPtr = reinterpret_cast<const void*>( simd );
    }

    template<typename T, typename... POS>
    static float32v GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        if( memberVariable.simdGeneratorPtr )
        {
            return EmitCall( memberVariable.simdGeneratorPtr, seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    static float32v GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        assert( memberVariable.simdGeneratorPtr );
        return EmitCall( memberVariable.simdGeneratorPtr, seed, pos... );
    }

    template<typename T>
    static const DispatchClass<T, FastNoise::Codegen::kFeatureSet>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable )
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        return static_cast<const DispatchClass<T, FastNoise::Codegen::kFeatureSet>*>( simdGen );
    }

    // The batch generation API has no meaning for a symbolic node
    FastNoise::OutputMinMax GenUniformGrid2D( float*, float, float, int, int, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenUniformGrid3D( float*, float, float, float, int, int, int, float, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenUniformGrid4D( float*, float, float, float, float, int, int, int, int, float, float, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenPositionArray2D( float*, int, const float*, const float*, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenPositionArray3D( float*, int, const float*, const float*, const float*, float, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenPositionArray4D( float*, int, const float*, const float*, const float*, const float*, float, float, float, float, int ) const final { assert( 0 ); return {}; }
    FastNoise::OutputMinMax GenTileable2D( float*, int, int, float, float, int ) const final { assert( 0 ); return {}; }
    float GenSingle2D( float, float, int ) const final { assert( 0 ); return 0; }
    float GenSingle3D( float, float, float, int ) const final { assert( 0 ); return 0; }
    float GenSingle4D( float, float, float, float, int ) const final { assert( 0 ); return 0; }

private:
    template<typename... POS>
    static float32v EmitCall( const void* simdGeneratorPtr, int32v seed, POS... pos )
    {
        const FastNoise::Generator* node = reinterpret_cast<VoidPtrStorageType>( simdGeneratorPtr );

        std::int32_t function = FastNoise::Codegen::gContext.tracer->RequestFunction( node, (int)sizeof...( POS ) );
        std::int32_t args[] = { seed.Id(), pos.Id()... };

        return float32v( FastNoise::Codegen::FromId{}, FastNoise::Codegen::ActiveBuilder().Call( function, args, 1 + (int)sizeof...( POS ) ) );
    }

    int32_t ReferencesFetchAdd( int32_t add ) const noexcept final
    {
        if( add )
        {
            return mReferences.fetch_add( add, std::memory_order_relaxed );
        }

        return mReferences.load( std::memory_order_relaxed );
    }

    mutable std::atomic<uint32_t> mReferences = 0;
};
