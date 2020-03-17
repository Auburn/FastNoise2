#pragma once
#include <functional>
#include <memory>
#include <type_traits>

#include "FastSIMD/FastSIMD.h"

namespace FastNoise
{
    class Generator;

    struct Metadata
    {
        Metadata() = delete;
        inline Metadata( const char* );

        //template<typename Ret, typename... Args>
        //static std::tuple<Args...> getArgs(Ret(*)(Args...));

        ////! Specialisation for Functor/Lambdas
        //template<typename F, typename Ret, typename... Args>
        //static std::tuple<Args...> getArgs(Ret(F::*)(Args...));

        //! Specialisation for Functor/Lambdas
        template<typename F, typename Ret, typename... Args>
        static std::tuple<Args...> getArgs(Ret(F::*)(Args...) const);

        template<typename F, std::size_t I>
        using GetArg = std::tuple_element_t<I, decltype(getArgs(&F::operator()))>;

        struct MemberVariable
        {
            enum eType
            {
                EFloat,
                EInt
            };

            union ValueUnion
            {
                float f;
                int32_t i;
            };

            const char* name;
            eType type;
            ValueUnion valueDefault, valueMin, valueMax;

            std::function<void(Generator*, ValueUnion)> setFunc;

            template<typename T>
            MemberVariable( const char* n, float defaultV, T&& func, float minV = 0.0f, float maxV = 0.0f )
            {
                name = n;
                type = EFloat;
                valueDefault.f = defaultV;
                valueMin.f = minV;
                valueMax.f = maxV;

                setFunc = [func]( Generator* g, ValueUnion v ) { func( dynamic_cast<GetArg<T, 0>>( g ), v.f ); };
            }

            template<typename T>
            MemberVariable(const char* n, float defaultV, void( T::*func )( float ), float minV = 0, float maxV = 0)
            {
                name = n;
                type = EFloat;
                valueDefault.f = defaultV;
                valueMin.f = minV;
                valueMax.f = maxV;

                setFunc = [func](Generator* g, ValueUnion v) { ( dynamic_cast<T*>(g)->*func )( v.f ); };
            }

            template<typename T>
            MemberVariable( const char* n, int32_t defaultV, T&& func, int32_t minV = 0, int32_t maxV = 0 )
            {
                name = n;
                type = EInt; 
                valueDefault.i = defaultV;
                valueMin.i = minV;
                valueMax.i = maxV;

                setFunc = [func]( Generator* g, ValueUnion v ) { func( dynamic_cast<GetArg<T, 0>>( g ), v.i ); };
            }

            template<typename T>
            MemberVariable( const char* n, int32_t defaultV, void( T::*func )( int32_t ), int32_t minV = 0, int32_t maxV = 0 )
            {
                name = n;
                type = EInt;
                valueDefault.i = defaultV;
                valueMin.i = minV;
                valueMax.i = maxV;

                setFunc = [func]( Generator* g, ValueUnion v ) { ( dynamic_cast<T*>(g)->*func )( v.i ); };
            }
        };

        struct MemberNode
        {
            const char* name;

            std::function<bool(Generator*, std::shared_ptr<Generator>&)> setFunc;

            template<typename T>
            MemberNode( const char* n, T&& func )
            {
                name = n;
                setFunc = [func]( Generator* g, std::shared_ptr<Generator>& s ) 
                {
                    std::remove_reference_t<GetArg<T, 1>> downCast = std::dynamic_pointer_cast<decltype( downCast )::element_type>( s );
                    if( downCast )
                    {
                        func( dynamic_cast<GetArg<T, 0>>( g ), downCast );
                    }
                    return (bool)downCast;
                };
            }

            template<typename T, typename U>
            MemberNode( const char* n, void(T::* func)( const std::shared_ptr<U>& ) )
            {
                name = n;
                setFunc = [func]( Generator* g, std::shared_ptr<Generator>& s ) 
                {
                    std::shared_ptr<U> downCast = std::dynamic_pointer_cast<U>( s );
                    if( downCast )
                    {
                        ( dynamic_cast<T*>( g )->*func )( downCast );
                    }
                    return (bool)downCast;
                };
            }
        };

        const char* name;
        uint16_t id;

        std::vector<MemberVariable> memberVariables;
        std::vector<MemberNode>     memberNodes;

        virtual Generator* NodeFactory( FastSIMD::eLevel ) const = 0;

        Generator* NodeFactory() const
        {
            return NodeFactory( FastSIMD::CPUMaxSIMDLevel() );
        }
    };

    class MetadataManager
    {
    public:
        static uint16_t AddMetadataClass( Metadata* newMetadata )
        {
            sMetadataClasses.emplace_back( newMetadata );

            return (uint16_t)sMetadataClasses.size() - 1;
        }

        static const std::vector<const Metadata*>& GetMetadataClasses()
        {
            return sMetadataClasses;
        }

    private:
        MetadataManager() = delete;

        static std::vector<const Metadata*> sMetadataClasses;
    };

    Metadata::Metadata( const char* className )
    {
        name = className;
        id = MetadataManager::AddMetadataClass( this );
    }

#define FASTNOISE_METADATA( ... ) public:\
    FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );\
    const FastNoise::Metadata* GetMetadata() override;\
    struct Metadata : __VA_ARGS__::Metadata{\
    virtual Generator* NodeFactory( FastSIMD::eLevel ) const override;

#define FASTNOISE_METADATA_ABSTRACT( ... ) public:\
    struct Metadata : __VA_ARGS__::Metadata{
}