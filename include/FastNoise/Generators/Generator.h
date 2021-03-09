#pragma once
#include <cassert>
#include <cmath>

#include "FastNoise/FastNoise_Config.h"

#if !defined( FASTNOISE_METADATA ) && defined( __INTELLISENSE__ )
#define FASTNOISE_METADATA
#endif

namespace FastNoise
{
    struct Metadata;
#ifdef FASTNOISE_METADATA
    template<typename T>
    struct MetadataT;
#endif

    enum class Dim
    {
        X, Y, Z, W,
        Count
    };

    constexpr static const char* kDim_Strings[] =
    {
        "X", "Y", "Z", "W",
    };

    enum class DistanceFunction
    {
        Euclidean,
        EuclideanSquared,
        Manhattan,
        Hybrid,
        MaxAxis,
    };

    constexpr static const char* kDistanceFunction_Strings[] =
    {
        "Euclidean",
        "Euclidean Squared",
        "Manhattan",
        "Hybrid",
        "Max Axis",
    };

    struct OutputMinMax
    {
        float min = INFINITY;
        float max = -INFINITY;

        OutputMinMax& operator <<( float v )
        {
            min = fminf( min, v );
            max = fmaxf( max, v );
            return *this;
        }

        OutputMinMax& operator <<( const OutputMinMax& v )
        {
            min = fminf( min, v.min );
            max = fmaxf( max, v.max );
            return *this;
        }
    };

    template<typename T>
    struct BaseSource
    {
        using Type = T;

        SmartNode<const T> base;
        const void* simdGeneratorPtr = nullptr;

    protected:
        BaseSource() = default;
    };

    template<typename T>
    struct GeneratorSourceT : BaseSource<T>
    { };

    template<typename T>
    struct HybridSourceT : BaseSource<T>
    {
        float constant;

        HybridSourceT( float f = 0.0f )
        {
            constant = f;
        }
    };

    class Generator
    {
    public:
#ifdef FASTNOISE_METADATA
        template<typename T>
        friend struct FastNoise::MetadataT;
#endif

        virtual ~Generator() = default;

        virtual FastSIMD::eLevel GetSIMDLevel() const = 0;
        virtual const Metadata& GetMetadata() const = 0;

        virtual OutputMinMax GenUniformGrid2D( float* noiseOut,
            int32_t xStart, int32_t yStart,
            int32_t xSize, int32_t ySize,
            float frequency, int32_t seed ) const = 0;

        virtual OutputMinMax GenUniformGrid3D( float* noiseOut,
            int32_t xStart, int32_t yStart, int32_t zStart,
            int32_t xSize,  int32_t ySize,  int32_t zSize,
            float frequency, int32_t seed ) const = 0;

        virtual OutputMinMax GenUniformGrid4D( float* noiseOut,
            int32_t xStart, int32_t yStart, int32_t zStart, int32_t wStart,
            int32_t xSize,  int32_t ySize,  int32_t zSize,  int32_t wSize,
            float frequency, int32_t seed ) const = 0;

        virtual OutputMinMax GenPositionArray2D( float* noiseOut, int32_t count,
            const float* xPosArray, const float* yPosArray,
            float xOffset, float yOffset, int32_t seed ) const = 0;

        virtual OutputMinMax GenPositionArray3D( float* noiseOut, int32_t count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, 
            float xOffset, float yOffset, float zOffset, int32_t seed ) const = 0;

        virtual OutputMinMax GenPositionArray4D( float* noiseOut, int32_t count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, 
            float xOffset, float yOffset, float zOffset, float wOffset, int32_t seed ) const = 0;

        virtual float GenSingle2D( float x, float y, int32_t seed ) const = 0;
        virtual float GenSingle3D( float x, float y, float z, int32_t seed ) const = 0;
        virtual float GenSingle4D( float x, float y, float z, float w, int32_t seed ) const = 0;

        virtual OutputMinMax GenTileable2D( float* noiseOut,
            int32_t xSize,  int32_t ySize, 
            float frequency, int32_t seed ) const = 0;  


    protected:
        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, SmartNodeArg<T> gen )
        {
            static_assert( std::is_base_of<Generator, T>::value, "T must be child of FastNoise::Generator class" );
            assert( gen.get() );
            assert( GetSIMDLevel() == gen->GetSIMDLevel() ); // Ensure that all SIMD levels match

            memberVariable.base = gen;
            SetSourceSIMDPtr( dynamic_cast<const Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
        }

    private:
        virtual void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) = 0;
    };

    using GeneratorSource = GeneratorSourceT<Generator>;
    using HybridSource = HybridSourceT<Generator>;

    template<typename T>
    struct PerDimensionVariable
    {
        using Type = T;

        T varArray[(int)Dim::Count];

        template<typename U = T>
        PerDimensionVariable( U value = 0 )
        {
            for( T& element : varArray )
            {
                element = value;
            }
        }

        T& operator[]( size_t i )
        {
            return varArray[i];
        }

        const T& operator[]( size_t i ) const
        {
            return varArray[i];
        }
    };

#ifdef FASTNOISE_METADATA

    template<>
    struct MetadataT<Generator> : Metadata
    {
    protected:
        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, U&& func, T minV = 0, T maxV = 0 )
        {
            MemberVariable member;
            member.name = name;
            member.valueDefault = defaultV;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* c = dynamic_cast<GetArg<U, 0>>(g) )
                {
                    func( c, v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, void(U::* func)(T), T minV = 0, T maxV = 0 )
        {
            MemberVariable member;
            member.name = name;
            member.valueDefault = defaultV;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* c = dynamic_cast<U*>(g) )
                {
                    (c->*func)( v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<std::is_enum_v<T>>, typename... ENUM_NAMES>
        void AddVariableEnum( const char* name, T defaultV, void(U::* func)(T), ENUM_NAMES... enumNames )
        {
            MemberVariable member;
            member.name = name;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int32_t)defaultV;
            member.enumNames = { enumNames... };

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* c = dynamic_cast<U*>(g) )
                {
                    (c->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, size_t ENUM_NAMES, typename = std::enable_if_t<std::is_enum_v<T>>>
        void AddVariableEnum( const char* name, T defaultV, void(U::* func)(T), const char* const (&enumNames)[ENUM_NAMES] )
        {
            MemberVariable member;
            member.name = name;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int32_t)defaultV;
            member.enumNames = { enumNames, enumNames + ENUM_NAMES };

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* c = dynamic_cast<U*>(g) )
                {
                    (c->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddPerDimensionVariable( const char* name, T defaultV, U&& func, T minV = 0, T maxV = 0 )
        {
            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<T>::varArray ) / sizeof( *PerDimensionVariable<T>::varArray ); idx++ )
            {
                MemberVariable member;
                member.name = name;
                member.valueDefault = defaultV;
                member.valueMin = minV;
                member.valueMax = maxV;

                member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, MemberVariable::ValueUnion v )
                {
                    if( auto* c = dynamic_cast<GetArg<U, 0>>(g) )
                    {
                        func( c ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberVariables.push_back( member );
            }
        }

        template<typename T, typename U>
        void AddGeneratorSource( const char* name, void(U::* func)(SmartNodeArg<T>) )
        {
            MemberNode member;
            member.name = name;

            member.setFunc = [func]( Generator* g, SmartNodeArg<> s )
            {
                if( SmartNode<const T> downCast = std::dynamic_pointer_cast<const T>( s ) )
                {
                    if( auto* c = dynamic_cast<U*>(g) )
                    {
                        (c->*func)( downCast );
                        return true;
                    }
                }
                return false;
            };

            memberNodes.push_back( member );
        }

        template<typename U>
        void AddPerDimensionGeneratorSource( const char* name, U&& func )
        {
            using GeneratorSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename GeneratorSourceT::Type;

            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<GeneratorSourceT>::varArray ) / sizeof( *PerDimensionVariable<GeneratorSourceT>::varArray ); idx++ )
            {
                MemberNode member;
                member.name = name;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( auto* g, SmartNodeArg<> s )
                {
                    if( SmartNode<const T> downCast = std::dynamic_pointer_cast<const T>( s ) )
                    {
                        if( auto* c = dynamic_cast<GetArg<U, 0>>(g) )
                        {
                            g->SetSourceMemberVariable( func( c ).get()[idx], downCast );
                            return true;
                        }
                    }
                    return false;
                };

                memberNodes.push_back( member );
            }
        }


        template<typename T, typename U>
        void AddHybridSource( const char* name, float defaultValue, void(U::* funcNode)(SmartNodeArg<T>), void(U::* funcValue)(float) )
        {
            MemberHybrid member;
            member.name = name;
            member.valueDefault = defaultValue;

            member.setNodeFunc = [funcNode]( auto* g, SmartNodeArg<> s )
            {
                if( SmartNode<const T> downCast = std::dynamic_pointer_cast<const T>( s ) )
                {
                    if( auto* c = dynamic_cast<U*>(g) )
                    {
                        (c->*funcNode)( downCast );
                        return true;
                    }
                }
                return false;
            };

            member.setValueFunc = [funcValue]( Generator* g, float v )
            {
                if( auto* c = dynamic_cast<U*>(g) )
                {
                    (c->*funcValue)( v );
                    return true;
                }                
                return false;
            };

            memberHybrids.push_back( member );
        }

        template<typename U>
        void AddPerDimensionHybridSource( const char* name, float defaultV, U&& func )
        {
            using HybridSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename HybridSourceT::Type;

            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<HybridSourceT>::varArray ) / sizeof( *PerDimensionVariable<HybridSourceT>::varArray ); idx++ )
            {
                MemberHybrid member;
                member.name = name;
                member.valueDefault = defaultV;
                member.dimensionIdx = idx;

                member.setNodeFunc = [func, idx]( auto* g, SmartNodeArg<> s )
                {
                    if( SmartNode<const T> downCast = std::dynamic_pointer_cast<const T>( s ) )
                    {
                        if( auto* c = dynamic_cast<GetArg<U, 0>>(g) )
                        {
                            g->SetSourceMemberVariable( func( c ).get()[idx], downCast );
                            return true;
                        }
                    }
                    return false;
                };

                member.setValueFunc = [func, idx]( Generator* g, float v )
                {
                    if( auto* c = dynamic_cast<GetArg<U, 0>>(g) )
                    {
                        func( c ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberHybrids.push_back( member );
            }
        }

    private:
        template<typename F, typename Ret, typename... Args>
        static std::tuple<Args...> GetArg_Helper( Ret( F::* )(Args...) const );

        template<typename F, std::size_t I>
        using GetArg = std::tuple_element_t<I, decltype(GetArg_Helper( &F::operator() ))>;
    };
#endif
}
