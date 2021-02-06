#pragma once
#include <functional>
#include <vector>
#include <cstdint>

#include "FastNoise_Config.h"

namespace FastNoise
{
    class Generator;
    template<typename T>
    struct PerDimensionVariable;
    struct NodeData;

    struct Metadata
    {
        Metadata()
        {
            id = AddMetadataClass( this );
        }

        virtual ~Metadata() = default;

        static const std::vector<const Metadata*>& GetMetadataClasses()
        {
            return sMetadataClasses;
        }

        static const Metadata* GetMetadataClass( std::uint16_t nodeId )
        {
            if( nodeId < sMetadataClasses.size() )
            {
                return sMetadataClasses[nodeId];
            }

            return nullptr;
        }

        static std::string SerialiseNodeData( NodeData* nodeData, bool fixUp = false );
        static NodeData* DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut );

        struct MemberVariable
        {
            enum eType
            {
                EFloat,
                EInt,
                EEnum
            };

            union ValueUnion
            {
                float f;
                std::int32_t i;

                ValueUnion( float v = 0 )
                {
                    f = v;
                }

                ValueUnion( std::int32_t v )
                {
                    i = v;
                }

                operator float()
                {
                    return f;
                }

                operator std::int32_t()
                {
                    return i;
                }

                bool operator ==( const ValueUnion& rhs ) const
                {
                    return i == rhs.i;
                }
            };

            const char* name;
            eType type;
            int dimensionIdx = -1;
            ValueUnion valueDefault, valueMin, valueMax;
            std::vector<const char*> enumNames;

            std::function<void( Generator*, ValueUnion )> setFunc;
        };

        struct MemberNode
        {
            const char* name;
            int dimensionIdx = -1;

            std::function<bool( Generator*, SmartNodeArg<> )> setFunc;
        };


        struct MemberHybrid
        {
            const char* name;
            float valueDefault = 0.0f;
            int dimensionIdx = -1;

            std::function<void( Generator*, float )> setValueFunc;
            std::function<bool( Generator*, SmartNodeArg<> )> setNodeFunc;
        };

        std::uint16_t id;
        const char* name;
        std::vector<const char*> groups;

        std::vector<MemberVariable> memberVariables;
        std::vector<MemberNode>     memberNodes;
        std::vector<MemberHybrid>   memberHybrids;

        virtual Generator* NodeFactory( FastSIMD::eLevel level = FastSIMD::Level_Null ) const = 0;

    private:
        static std::uint16_t AddMetadataClass( const Metadata* newMetadata )
        {
            sMetadataClasses.emplace_back( newMetadata );

            return (std::uint16_t)sMetadataClasses.size() - 1;
        }

        static std::vector<const Metadata*> sMetadataClasses;
    };

    struct NodeData
    {
        NodeData( const Metadata* metadata );

        const Metadata* metadata;
        std::vector<Metadata::MemberVariable::ValueUnion> variables;
        std::vector<NodeData*> nodes;
        std::vector<std::pair<NodeData*, float>> hybrids;

        bool operator ==( const NodeData& rhs ) const
        {
            return metadata == rhs.metadata &&
                variables == rhs.variables &&
                nodes == rhs.nodes &&
                hybrids == rhs.hybrids;
        }
    };
}
