#pragma once
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

#include "FastNoise_Config.h"

namespace FastNoise
{
    class Generator;
    template<typename T>
    struct PerDimensionVariable;
    struct NodeData;

    // Stores definition of a FastNoise node class
    // Node name, member name+types, functions to set members
    struct Metadata
    {
        virtual ~Metadata() = default;

        /// <returns>Array containing metadata for every FastNoise node type</returns>
        static const std::vector<const Metadata*>& GetMetadataClasses()
        {
            return sMetadataClasses;
        }

        /// <returns>Metadata for given Metadata::id</returns>
        static const Metadata* GetMetadataClass( std::uint16_t nodeId )
        {
            if( nodeId < sMetadataClasses.size() )
            {
                return sMetadataClasses[nodeId];
            }

            return nullptr;
        }

        /// <summary>
        /// Serialise node data and any source node datas (recursive)
        /// </summary>
        /// <param name="nodeData">Root node data</param>
        /// <param name="fixUp">Remove dependency loops and invalid node types</param>
        /// <returns>Empty string on error</returns>
        static std::string SerialiseNodeData( NodeData* nodeData, bool fixUp = false );

        /// <summary>
        /// Deserialise a string created from SerialiseNodeData to a node data tree
        /// </summary>
        /// <param name="serialisedBase64NodeData">Encoded string to deserialise</param>
        /// <param name="nodeDataOut">Storage for new node data</param>
        /// <returns>Root node</returns>
        static NodeData* DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut );

        // Base member struct
        struct Member
        {
            const char* name;
            int dimensionIdx = -1;            
        };

        /// <summary>
        /// Add spaces to node names: DomainScale -> Domain Scale
        /// </summary>
        /// <param name="metadata">FastNoise node metadata</param>
        /// <param name="removeGroups">Removes metadata groups from name: FractalFBm -> FBm</param>
        /// <returns>string with formatted name</returns>
        static std::string FormatMetadataNodeName( const Metadata* metadata, bool removeGroups = false );

        /// <summary>
        /// Adds dimension prefix to member varibles that per-dimension:
        /// DomainAxisScale::Scale -> X Scale
        /// </summary>
        /// <param name="member">FastNoise node metadata member</param>
        /// <returns>string with formatted name</returns>
        static std::string FormatMetadataMemberName( const Member& member );

        // float, int or enum value
        struct MemberVariable : Member
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

            eType type;
            ValueUnion valueDefault, valueMin, valueMax;
            std::vector<const char*> enumNames;

            // Function to set value for given generator
            // Returns true if Generator is correct node class
            std::function<bool( Generator*, ValueUnion )> setFunc;
        };

        // Node lookup (must be valid for node to function)
        struct MemberNode : Member
        {
            // Function to set source for given generator
            // Returns true if Generator* is correct node class and SmartNodeArg<> is correct node class
            std::function<bool( Generator*, SmartNodeArg<> )> setFunc;
        };

        // Either a constant float or node lookup
        struct MemberHybrid : Member
        {
            float valueDefault = 0.0f;

            // Function to set value for given generator
            // Returns true if Generator is correct node class
            std::function<bool( Generator*, float )> setValueFunc;

            // Function to set source for given generator
            // Source takes priority if value is also set
            // Returns true if Generator is correct node class and SmartNodeArg<> is correct node class
            std::function<bool( Generator*, SmartNodeArg<> )> setNodeFunc;
        };

        std::uint16_t id;
        const char* name;
        std::vector<const char*> groups;

        std::vector<MemberVariable> memberVariables;
        std::vector<MemberNode>     memberNodes;
        std::vector<MemberHybrid>   memberHybrids;

        /// <summary>
        /// Create new instance of a FastNoise node from metadata
        /// </summary>
        /// <example>
        /// auto node = metadata->CreateNode();
        /// metadata->memberVariables[0].setFunc( node.get(), 1.5f );
        /// </example>
        /// <param name="maxSimdLevel">Max SIMD level, Null = Auto</param>
        /// <returns>SmartNode<T> is guaranteed not nullptr</returns>
        virtual SmartNode<> CreateNode( FastSIMD::eLevel maxSimdLevel = FastSIMD::Level_Null ) const = 0;

    protected:
        Metadata()
        {
            id = AddMetadataClass( this );
        }

    private:
        static std::uint16_t AddMetadataClass( const Metadata* newMetadata )
        {
            sMetadataClasses.emplace_back( newMetadata );

            return (std::uint16_t)sMetadataClasses.size() - 1;
        }

        static std::vector<const Metadata*> sMetadataClasses;
    };

    // Stores data to create an instance of a FastNoise node
    // Node type, member values
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
