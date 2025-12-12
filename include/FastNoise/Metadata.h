#pragma once
#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

#include "Utility/Config.h"

namespace FastNoise
{
    class Generator;
    template<typename T>
    struct PerDimensionVariable;
    struct Metadata;
    struct NodeData;

    namespace Impl
    {
        template<typename T>
        FASTNOISE_API const Metadata& GetMetadata();
    }

    // Stores definition of a FastNoise node class
    // Node name, member name+types, functions to set members
    struct FASTNOISE_API Metadata
    {
        template<typename T>
        class FASTNOISE_API Vector
        {
        public:
            using const_iterator = const T*;

            const_iterator begin() const { return data() + mStart; }
            const_iterator end() const { return data() + mEnd; }
            size_t size() const { return mEnd - mStart; }
            const T& operator []( size_t i ) const { return begin()[i]; }

        private:
            template<typename>
            friend struct MetadataT;
            friend struct Metadata;
            friend class Generator;
            using index_type = uint8_t;

            T* data() const;
            void push_back( const T& value );

            index_type mStart = (index_type)-1;
            index_type mEnd = (index_type)-1;
        };

        struct NameDesc
        {
            const char* name;
            const char* desc;
            
            NameDesc( const char* name, const char* desc = "" ) : name( name ), desc( desc ) {}
        };

        // Base member struct
        struct Member
        {
            const char* name = "";
            const char* description = "";
            int dimensionIdx = -1;
        };

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
                int i;

                ValueUnion( float v = 0 )
                {
                    f = v;
                }

                ValueUnion( int v )
                {
                    i = v;
                }

                operator float()
                {
                    return f;
                }

                operator int()
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
            float valueUiDragSpeed = 0;
            Vector<const char*> enumNames;

            // Function to set value for given generator
            // Returns true if Generator is correct node class
            std::function<bool( Generator*, ValueUnion )> setFunc;
        };

        // Node lookup (must be valid for node to function)
        struct MemberNodeLookup : Member
        {
            // Function to set source for given generator
            // Returns true if Generator* is correct node class and SmartNodeArg<> is correct node class
            std::function<bool( Generator*, SmartNodeArg<> )> setFunc;
        };

        // Either a constant float or node lookup
        struct MemberHybrid : Member
        {
            float valueDefault, valueUiDragSpeed;

            // Function to set value for given generator
            // Returns true if Generator is correct node class
            std::function<bool( Generator*, float )> setValueFunc;

            // Function to set source for given generator
            // Source takes priority if value is also set
            // Returns true if Generator is correct node class and SmartNodeArg<> is correct node class
            std::function<bool( Generator*, SmartNodeArg<> )> setNodeFunc;
        };

        using node_id = uint8_t;

        static std::pair<int32_t, const char*> DebugCheckVectorStorageSize( int i );

        virtual ~Metadata() = default;

        /// <returns>Array containing metadata for every FastNoise node type</returns>
        static const Vector<const Metadata*>& GetAll()
        {
            return sAllMetadata;
        }

        /// <returns>Metadata for given Metadata::id</returns>
        static const Metadata* GetFromId( node_id nodeId )
        {
            // Metadata not loaded yet
            // Don't try to create nodes from metadata during static initialisation
            // Metadata is loaded using static variable and static variable init is done in a random order
            assert( sAllMetadata.size() );

            if( nodeId < sAllMetadata.size() )
            {
                return sAllMetadata[nodeId];
            }

            return nullptr;
        }

        /// <returns>Metadata for given node class</returns>
        template<typename T>
        static const Metadata& Get()
        {
            static_assert( std::is_base_of<Generator, T>::value, "This function should only be used for FastNoise node classes, for example FastNoise::Simplex" );
            static_assert( std::is_member_function_pointer<decltype(&T::GetMetadata)>::value, "Cannot get Metadata for abstract node class, use a derived class, for example: Fractal -> FractalFBm" );

            return Impl::GetMetadata<T>();
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

        /// <summary>
        /// Create new instance of a FastNoise node from metadata
        /// </summary>
        /// <example>
        /// auto node = metadata->CreateNode();
        /// metadata->memberVariables[0].setFunc( node.get(), 1.5f );
        /// </example>
        /// <param name="maxSimdLevel">Max SIMD level, Null = Auto</param>
        /// <returns>SmartNode<T> is guaranteed not nullptr</returns>
        virtual SmartNode<> CreateNode( FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max ) const = 0;

        node_id id;
        Vector<MemberVariable>   memberVariables;
        Vector<MemberNodeLookup> memberNodeLookups;
        Vector<MemberHybrid>     memberHybrids;
        Vector<const char*>      groups;

        const char* name = "";
        const char* description = "";
        const char* formattedName = nullptr;

    protected:
        Metadata()
        {
            id = AddMetadata( this );
        }

        static constexpr float kDefaultUiDragSpeedFloat = 0.02f;
        static constexpr float kDefaultUiDragSpeedInt = 0.2f;

    private:
        static node_id AddMetadata( const Metadata* newMetadata )
        {
            sAllMetadata.push_back( newMetadata );

            return (node_id)sAllMetadata.size() - 1;
        }

        static Vector<const Metadata*> sAllMetadata;
    };

    // Stores data to create an instance of a FastNoise node
    // Node type, member values
    struct NodeData
    {
        NodeData( const Metadata* data )
        {
            if( ( metadata = data ) )
            {
                for( const Metadata::MemberVariable& value: metadata->memberVariables )
                {
                    variables.push_back( value.valueDefault );
                }

                nodeLookups.assign( metadata->memberNodeLookups.size(), nullptr );

                for( const Metadata::MemberHybrid& value: metadata->memberHybrids )
                {
                    hybrids.emplace_back( nullptr, value.valueDefault );
                }
            }
        }

        const Metadata* metadata;
        std::vector<Metadata::MemberVariable::ValueUnion> variables;
        std::vector<NodeData*> nodeLookups;
        std::vector<std::pair<NodeData*, float>> hybrids;

        bool operator ==( const NodeData& rhs ) const
        {
            return metadata == rhs.metadata &&
                variables == rhs.variables &&
                nodeLookups == rhs.nodeLookups &&
                hybrids == rhs.hybrids;
        }
    };
}
