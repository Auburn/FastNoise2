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

    /** @brief Runtime reflection data for a FastNoise node class.
     *
     *  Each concrete node type (e.g. Simplex, FractalFBm) has a corresponding Metadata
     *  instance that describes its name, parameters, source inputs, and how to create
     *  and configure instances at runtime. This powers serialisation/deserialisation,
     *  the Node Editor UI, and language bindings.
     *
     *  Use Metadata::GetAll() to enumerate all registered node types, or
     *  Metadata::Get<T>() to get the metadata for a specific node class.
     *
     *  The auto-generated wiki Node reference pages are populated from metadata descriptions.
     *
     *  @see NodeData, MetadataT
     */
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

        /** @brief Name and description pair used when registering node parameters. */
        struct NameDesc
        {
            const char* name;
            const char* desc;
            
            NameDesc( const char* name, const char* desc = "" ) : name( name ), desc( desc ) {}
        };

        /** @brief Base struct for all metadata member descriptors (variables, sources, hybrids).
         *  @see MemberVariable, MemberNodeLookup, MemberHybrid
         */
        struct Member
        {
            const char* name = "";        ///< Display name of the member.
            const char* description = ""; ///< Human-readable description shown in UI/docs.
            int dimensionIdx = -1;        ///< Dimension index for per-dimension members, or -1 if not per-dimension.
        };

        /** @brief Describes a float, int, or enum parameter on a node.
         *
         *  Stores the parameter type, default/min/max values, and a function to apply
         *  the value to a Generator instance at runtime.
         */
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

            /** @brief Set the value on a generator instance.
             *  @return true if the generator was the correct node class for this member.
             */
            std::function<bool( Generator*, ValueUnion )> setFunc;
        };

        /** @brief Describes a required generator source input on a node.
         *
         *  Represents a connection that must be wired to another Generator node
         *  for the owning node to function (e.g. Fractal source, DomainWarp source).
         */
        struct MemberNodeLookup : Member
        {
            /** @brief Connect a source generator to this input.
             *  @return true if both the target generator and source node are the correct types.
             */
            std::function<bool( Generator*, SmartNodeArg<> )> setFunc;
        };

        /** @brief Describes a hybrid input that accepts either a constant float or a generator node.
         *
         *  When a generator node is connected it takes priority over the constant value.
         *  This allows parameters like fractal gain or warp amplitude to be driven by
         *  another noise source for spatial variation, or set to a simple constant.
         */
        struct MemberHybrid : Member
        {
            float valueDefault;     ///< Default constant value.
            float valueUiDragSpeed; ///< UI drag speed hint.

            /** @brief Set the constant float value.
             *  @return true if the generator was the correct node class.
             */
            std::function<bool( Generator*, float )> setValueFunc;

            /** @brief Connect a generator node as the source (overrides constant value).
             *  @return true if both the generator and source node are the correct types.
             */
            std::function<bool( Generator*, SmartNodeArg<> )> setNodeFunc;
        };

        using node_id = uint8_t;

        static std::pair<int32_t, const char*> DebugCheckVectorStorageSize( int i );

        virtual ~Metadata() = default;

        /** @brief Get the metadata for every registered FastNoise node type.
         *  @return Array of pointers to all node metadata, indexed by node_id.
         */
        static const Vector<const Metadata*>& GetAll()
        {
            return sAllMetadata;
        }

        /** @brief Look up metadata by its numeric node ID.
         *  @param nodeId  The ID to look up (corresponds to Metadata::id).
         *  @return Pointer to the metadata, or nullptr if the ID is out of range.
         *  @warning Do not call during static initialisation; metadata registration order is undefined.
         */
        static const Metadata* GetFromId( node_id nodeId )
        {
            assert( sAllMetadata.size() );

            if( nodeId < sAllMetadata.size() )
            {
                return sAllMetadata[nodeId];
            }

            return nullptr;
        }

        /** @brief Get the metadata for a specific node class at compile time.
         *  @tparam T  Concrete node class (e.g. FastNoise::Simplex). Must not be abstract.
         *  @return Reference to the metadata for node type T.
         */
        template<typename T>
        static const Metadata& Get()
        {
            static_assert( std::is_base_of<Generator, T>::value, "This function should only be used for FastNoise node classes, for example FastNoise::Simplex" );
            static_assert( std::is_member_function_pointer<decltype(&T::GetMetadata)>::value, "Cannot get Metadata for abstract node class, use a derived class, for example: Fractal -> FractalFBm" );

            return Impl::GetMetadata<T>();
        }

        /** @brief Serialise a node data tree to an encoded string.
         *
         *  Recursively serialises the root node and all of its source nodes.
         *  The resulting string can be deserialised with DeserialiseNodeData() or
         *  loaded directly with FastNoise::NewFromEncodedNodeTree().
         *
         *  @param nodeData  Root node data to serialise.
         *  @param fixUp     If true, removes dependency loops and invalid node types before serialising.
         *  @return Encoded string, or an empty string on error.
         */
        static std::string SerialiseNodeData( NodeData* nodeData, bool fixUp = false );

        /** @brief Deserialise an encoded string back into a node data tree.
         *
         *  @param serialisedBase64NodeData  Encoded string previously created by SerialiseNodeData().
         *  @param[out] nodeDataOut          Vector that receives ownership of all allocated NodeData objects.
         *  @return Pointer to the root NodeData in the tree, or nullptr on failure.
         */
        static NodeData* DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut );

        /** @brief Format a node class name for display by inserting spaces at word boundaries.
         *
         *  For example: `DomainScale` becomes `"Domain Scale"`.
         *
         *  @param metadata      The node metadata to format.
         *  @param removeGroups  If true, strips group prefixes from the name (e.g. `FractalFBm` becomes `"FBm"`).
         *  @return The formatted display name.
         */
        static std::string FormatMetadataNodeName( const Metadata* metadata, bool removeGroups = false );

        /** @brief Format a member name for display, adding a dimension prefix for per-dimension members.
         *
         *  For example: `DomainAxisScale::Scale` with dimensionIdx 0 becomes `"X Scale"`.
         *
         *  @param member  The member descriptor to format.
         *  @return The formatted display name.
         */
        static std::string FormatMetadataMemberName( const Member& member );

        /** @brief Create a new generator node instance from this metadata.
         *
         *  This allows creating nodes dynamically at runtime without compile-time type knowledge.
         *  Configure the node by calling the setFunc on memberVariables, memberNodeLookups,
         *  and memberHybrids.
         *
         *  @code
         *  auto node = metadata->CreateNode();
         *  metadata->memberVariables[0].setFunc( node.get(), 1.5f );
         *  @endcode
         *
         *  @param maxFeatureSet  Maximum SIMD feature set to use. Defaults to auto-detect.
         *  @return A SmartNode owning the new generator, or null if maxFeatureSet is below the min compiled SIMD feature set.
         */
        virtual SmartNode<> CreateNode( FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max ) const = 0;

        node_id id;                                ///< Unique numeric identifier for this node type.
        Vector<MemberVariable>   memberVariables;  ///< Float, int, and enum parameters.
        Vector<MemberNodeLookup> memberNodeLookups; ///< Required generator source inputs.
        Vector<MemberHybrid>     memberHybrids;    ///< Hybrid inputs (constant float or generator).
        Vector<const char*>      groups;           ///< Category groups (e.g. "Coherent Noise", "Fractal").

        const char* name = "";            ///< Node class name (e.g. "Simplex", "FractalFBm").
        const char* description = "";     ///< Human-readable description of the node's behaviour.
        const char* formattedName = nullptr; ///< Cached formatted display name, or nullptr if not yet formatted.

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

    /** @brief Serialisable data representation of a FastNoise node instance.
     *
     *  Stores the node type (via its Metadata pointer) and all configured parameter
     *  values, source connections, and hybrid inputs. Used as an intermediate format
     *  for serialisation/deserialisation of node trees.
     *
     *  @see Metadata::SerialiseNodeData, Metadata::DeserialiseNodeData
     */
    struct NodeData
    {
        /** @brief Construct a NodeData from metadata, initialising all members to their defaults.
         *  @param data  Metadata describing the node type. May be nullptr.
         */
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

        const Metadata* metadata; ///< The node type this data represents.
        std::vector<Metadata::MemberVariable::ValueUnion> variables; ///< Current values for each MemberVariable.
        std::vector<NodeData*> nodeLookups; ///< Connected source nodes (parallel to Metadata::memberNodeLookups).
        std::vector<std::pair<NodeData*, float>> hybrids; ///< Hybrid inputs: (source node or nullptr, constant value).

        bool operator ==( const NodeData& rhs ) const
        {
            return metadata == rhs.metadata &&
                variables == rhs.variables &&
                nodeLookups == rhs.nodeLookups &&
                hybrids == rhs.hybrids;
        }
    };
}
