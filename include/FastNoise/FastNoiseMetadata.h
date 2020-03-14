#pragma once
#include <functional>
#include <memory>
#include <type_traits>

namespace FastNoise
{
    class Generator;

    struct Metadata
    {
        Metadata() = delete;
        inline Metadata( const char* );

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

            MemberVariable( const char* n, float defaultV, std::function<void(Generator*,float)>&& func, float minV = 0.0f, float maxV = 0.0f )
            {
                name = n;
                type = EFloat;
                valueDefault.f = defaultV;
                valueMin.f = minV;
                valueMax.f = maxV;

                setFunc = [func]( Generator* g, ValueUnion v ) { func( g, v.f ); };
            }

            MemberVariable( const char* n, int32_t defaultV, std::function<void(Generator*, int32_t)>&& func, int32_t minV = 0, int32_t maxV = 0 )
            {
                name = n;
                type = EInt;
                valueDefault.i = defaultV;
                valueMin.i = minV;
                valueMax.i = maxV;

                setFunc = [func]( Generator* g, ValueUnion v ) { func( g, v.i ); };
            }
        };

        struct MemberNode
        {            
            const char* name;

            std::function<bool(Generator*, std::shared_ptr<Generator>&)> setFunc;

            MemberNode( const char* n, std::function<bool(Generator*, std::shared_ptr<Generator>&)>&& func )
            {
                name = n;
                setFunc = func;
            }
        };

        const char* name;
        uint16_t id;

        std::vector<MemberVariable> memberVariables;
        std::vector<MemberNode>     memberNodes;
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
    FastNoise::Metadata* GetMetadata() override;\
    struct Metadata : __VA_ARGS__::Metadata

#define FASTNOISE_METADATA_ABSTRACT( ... ) public:\
    struct Metadata : __VA_ARGS__::Metadata
}