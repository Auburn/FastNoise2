#pragma once
#include <cassert>
#include <cmath>
#include <algorithm>
#include <atomic>

#ifdef FASTNOISE_METADATA
#include <tuple>
#endif

#include "FastNoise/Utility/Config.h"

#if !defined( FASTNOISE_METADATA ) && ( defined( __INTELLISENSE__ ) || defined( __CLION_IDE__ ) )
#define FASTNOISE_METADATA
#endif

namespace FastNoise
{
    // Dimension
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
        Minkowski,
    };

    constexpr static const char* kDistanceFunction_Strings[] =
    {
        "Euclidean",
        "Euclidean Squared",
        "Manhattan",
        "Hybrid",
        "Max Axis",
        "Minkowski",
    };

    struct OutputMinMax
    {
        float min =  INFINITY;
        float max = -INFINITY;

        OutputMinMax& operator <<( float v )
        {
            min = std::min( min, v );
            max = std::max( max, v );
            return *this;
        }

        OutputMinMax& operator <<( const OutputMinMax& v )
        {
            min = std::min( min, v.min );
            max = std::max( max, v.max );
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

        constexpr HybridSourceT( float f = 0.0f ) : constant( f ) { }
    };

    namespace Internal
    {
        void BumpNodeRefences( const Generator*, bool );
    }

    class FASTNOISE_API Generator
    {
    public:
        template<typename T>
        friend struct MetadataT;

        Generator() = default;
        Generator( const Generator& ) = delete;
        Generator( Generator&& ) = delete;

        virtual ~Generator() = default;

        virtual FastSIMD::FeatureSet GetActiveFeatureSet() const = 0;
        virtual const Metadata& GetMetadata() const = 0;

        virtual OutputMinMax GenUniformGrid2D( float* out,
            int xStart, int yStart,
            int xSize,  int ySize,
            int seed ) const = 0;

        virtual OutputMinMax GenUniformGrid3D( float* out,
            int xStart, int yStart, int zStart, 
            int xSize,  int ySize,  int zSize, 
            int seed ) const = 0;

        virtual OutputMinMax GenUniformGrid4D( float* out,
            int xStart, int yStart, int zStart, int wStart,
            int xSize,  int ySize,  int zSize,  int wSize,
            int seed ) const = 0;

        virtual OutputMinMax GenTileable2D( float* out,
            int xSize, int ySize, int seed ) const = 0; 

        virtual OutputMinMax GenPositionArray2D( float* out, int count,
            const float* xPosArray, const float* yPosArray,
            float xOffset, float yOffset, int seed ) const = 0;

        virtual OutputMinMax GenPositionArray3D( float* out, int count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, 
            float xOffset, float yOffset, float zOffset, int seed ) const = 0;

        virtual OutputMinMax GenPositionArray4D( float* out, int count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, 
            float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const = 0;

        virtual float GenSingle2D( float x, float y, int seed ) const = 0;
        virtual float GenSingle3D( float x, float y, float z, int seed ) const = 0;
        virtual float GenSingle4D( float x, float y, float z, float w, int seed ) const = 0;

    protected:
        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, SmartNodeArg<T> gen )
        {
            static_assert( std::is_base_of<Generator, T>::value, "T must be child of FastNoise::Generator class" );

            assert( !gen.get() || GetActiveFeatureSet() == gen->GetActiveFeatureSet() ); // Ensure that all SIMD levels match

            SetSourceSIMDPtr( static_cast<const Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
            memberVariable.base = gen;
        }

    private:
        virtual void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) = 0;
        virtual int32_t ReferencesFetchAdd( int32_t add = 0 ) const noexcept = 0;

        template<typename>
        friend class SmartNode;
        friend void Internal::BumpNodeRefences( const Generator*, bool );
    };

    using GeneratorSource = GeneratorSourceT<Generator>;
    using HybridSource = HybridSourceT<Generator>;

    template<typename T>
    struct PerDimensionVariable
    {
        using Type = T;

        T varArray[(int)Dim::Count];

        template<typename U = T>
#if __cplusplus >= 201402L
        constexpr
#endif
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
        void AddVariable( NameDesc nameDesc, T defaultV, U&& func, T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultV;
            member.valueUiDragSpeed = uiDragSpeed;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                {
                    func( gRealType, v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename V, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( NameDesc nameDesc, T defaultV, V ( U::*func )( T ), T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultV;
            member.valueUiDragSpeed = uiDragSpeed;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<std::is_enum_v<T>>, typename... ENUM_NAMES>
        void AddVariableEnum( NameDesc nameDesc, T defaultV, void(U::* func)(T), ENUM_NAMES... enumNames )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            ( member.enumNames.push_back( enumNames ), ... );

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, size_t ENUM_NAMES, typename = std::enable_if_t<std::is_enum_v<T>>>
        void AddVariableEnum( NameDesc nameDesc, T defaultV, void(U::* func)(T), const char* const (&enumNames)[ENUM_NAMES] )
        {
            MemberVariable member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            for( const char* enumName : enumNames )
            {
                member.enumNames.push_back( enumName );
            }

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*func)( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddPerDimensionVariable( NameDesc nameDesc, T defaultV, U&& func, T minV = 0, T maxV = 0, float uiDragSpeed = std::is_same_v<T, float> ? Metadata::kDefaultUiDragSpeedFloat : Metadata::kDefaultUiDragSpeedInt )
        {
            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberVariable member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.valueDefault = defaultV;
                member.valueUiDragSpeed = uiDragSpeed;
                member.valueMin = minV;
                member.valueMax = maxV;

                member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, MemberVariable::ValueUnion v )
                {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberVariables.push_back( member );
            }
        }

        template<typename T, typename U>
        void AddGeneratorSource( NameDesc nameDesc, void(U::* func)(SmartNodeArg<T>) )
        {
            MemberNodeLookup member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;

            member.setFunc = [func]( Generator* g, SmartNodeArg<> s )
            {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast ); 
                        (gRealType->*func)( source );
                        return true;
                    }
                }
                return false;
            };

            memberNodeLookups.push_back( member );
        }

        template<typename U>
        void AddPerDimensionGeneratorSource( NameDesc nameDesc, U&& func )
        {
            using GeneratorSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename GeneratorSourceT::Type;

            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberNodeLookup member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, SmartNodeArg<> s )
                {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast ); 
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                memberNodeLookups.push_back( member );
            }
        }


        template<typename T, typename U>
        void AddHybridSource( NameDesc nameDesc, float defaultValue, void ( U::*funcNode )( SmartNodeArg<T> ), void ( U::*funcValue )( float ), float uiDragSpeed = Metadata::kDefaultUiDragSpeedFloat )
        {
            MemberHybrid member;
            member.name = nameDesc.name;
            member.description = nameDesc.desc;
            member.valueDefault = defaultValue;
            member.valueUiDragSpeed = uiDragSpeed;

            member.setNodeFunc = [funcNode]( Generator* g, SmartNodeArg<> s )
            {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast ); 
                        (gRealType->*funcNode)( source );
                        return true;
                    }
                }
                return false;
            };

            member.setValueFunc = [funcValue]( Generator* g, float v )
            {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    (gRealType->*funcValue)( v );
                    return true;
                }                
                return false;
            };

            memberHybrids.push_back( member );
        }

        template<typename U>
        void AddPerDimensionHybridSource( NameDesc nameDesc, float defaultV, U&& func, float uiDragSpeed = Metadata::kDefaultUiDragSpeedFloat )
        {
            using HybridSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename HybridSourceT::Type;

            for( int idx = 0; (size_t)idx < (size_t)Dim::Count; idx++ )
            {
                MemberHybrid member;
                member.name = nameDesc.name;
                member.description = nameDesc.desc;
                member.valueDefault = defaultV;
                member.valueUiDragSpeed = uiDragSpeed;
                member.dimensionIdx = idx;

                member.setNodeFunc = [func, idx]( Generator* g, SmartNodeArg<> s )
                {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast ); 
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                member.setValueFunc = [func, idx]( Generator* g, float v )
                {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
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

        template<typename F, size_t I>
        using GetArg = std::tuple_element_t<I, decltype(GetArg_Helper( &F::operator() ))>;
    };
#endif
}
