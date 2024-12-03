#define FASTNOISE_METADATA // Should only be defined here

#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <limits>
#include <cassert>
#include <cstdint>

#include "FastNoise/Metadata.h"
#include "FastNoise/FastNoise.h"
#include "Base64.h"

using namespace FastNoise;

Metadata::Vector<const Metadata*> Metadata::sAllMetadata;

template<typename T>
constexpr static std::nullptr_t gMetadataVectorSize = nullptr; // Invalid

// Setting these values avoids needless vector resizing and oversizing on startup
// Sadly there is no way to automate this as they fill up as part of static init
template<>
constexpr size_t gMetadataVectorSize<const Metadata*> = 46;
template<>
constexpr size_t gMetadataVectorSize<const char*> = 84;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberVariable> = 71;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberNodeLookup> = 30;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberHybrid> = 56;

template<typename T>
static std::vector<T>& GetVectorStorage()
{
    static std::vector<T> v = []()
    {
        std::vector<T> vec;
        vec.reserve( gMetadataVectorSize<T> );
        return vec;
    }();
    return v;
}

template<typename T>
static int32_t DebugCheckType()
{
    return ( GetVectorStorage<T>().size() == gMetadataVectorSize<T> ? -1 : 1 ) * (int32_t)GetVectorStorage<T>().size();
}

std::pair<int32_t, const char*> Metadata::DebugCheckVectorStorageSize( int i )
{
    switch( i )
    {
    case 0: return { DebugCheckType<const Metadata*>(),  "const Metadata*" };
    case 1: return { DebugCheckType<const char*>(),      "const char*" };
    case 2: return { DebugCheckType<MemberVariable>(),   "MemberVariable" };
    case 3: return { DebugCheckType<MemberNodeLookup>(), "MemberNodeLookup" };
    case 4: return { DebugCheckType<MemberHybrid>(),     "MemberHybrid" };
    }
    return { 0, nullptr };
}

template<typename T>
T* Metadata::Vector<T>::data() const
{
    return GetVectorStorage<T>().data();
}

template<typename T>
void Metadata::Vector<T>::push_back( const T& value )
{
    std::vector<T>& vec = GetVectorStorage<T>();
    vec.push_back( value );
    assert( vec.size() <= (index_type)-1 );

    mEnd = (index_type)vec.size() - 1;
    mStart = std::min( mStart, mEnd++ );
}

template class Metadata::Vector<const Metadata*>;
template class Metadata::Vector<const char*>;
template class Metadata::Vector<Metadata::MemberVariable>;
template class Metadata::Vector<Metadata::MemberNodeLookup>;
template class Metadata::Vector<Metadata::MemberHybrid>;

union MemberLookup
{
    struct
    {
        uint8_t type : 2;
        uint8_t index : 6;
    } member;

    uint8_t data;
};

template<typename T>
static void AddToDataStream( std::vector<uint8_t>& dataStream, T value )
{
    for( size_t i = 0; i < sizeof( T ); i++ )
    {
        dataStream.push_back( (uint8_t)(value >> (i * 8)) );        
    }
}

static void AddMemberLookupToDataStream( std::vector<uint8_t>& dataStream, uint8_t type, uint8_t index )
{
    MemberLookup memberLookup;
    memberLookup.member.type = type;
    memberLookup.member.index = index;
    AddToDataStream( dataStream, memberLookup.data );
}


static bool SerialiseNodeDataInternal( NodeData* nodeData, bool fixUp, std::vector<uint8_t>& dataStream, std::unordered_map<const NodeData*, uint16_t>& referenceIds, std::unordered_set<const NodeData*> dependencies = {} )
{
    // dependencies passed by value to avoid false positives from other branches in the node tree

    const Metadata* metadata = nodeData->metadata;

    if( !metadata ||
        nodeData->variables.size() != metadata->memberVariables.size() ||
        nodeData->nodeLookups.size() != metadata->memberNodeLookups.size() ||
        nodeData->hybrids.size() != metadata->memberHybrids.size() )
    {
        assert( 0 ); // Member size mismatch with metadata
        return false;
    }

    if( fixUp )
    {
        dependencies.insert( nodeData );

        // Null any dependency loops 
        for( auto& node : nodeData->nodeLookups )
        {
            if( dependencies.find( node ) != dependencies.end() )
            {
                node = nullptr;
            }
        }
        for( auto& hybrid : nodeData->hybrids )
        {
            if( dependencies.find( hybrid.first ) != dependencies.end() )
            {
                hybrid.first = nullptr;
            }
        }
    }

    // Reference previously encoded nodes to reduce encoded string length
    // Relevant if a node has multiple links from it's output
    auto reference = referenceIds.find( nodeData );

    if( reference != referenceIds.end() )
    {
        // UINT8_MAX where node ID should be
        // Referenced by index in reference array, array ordering will match on decode
        AddToDataStream( dataStream, std::numeric_limits<Metadata::node_id>::max() );
        AddToDataStream( dataStream, reference->second );
        return true;
    }

    // Node ID
    AddToDataStream( dataStream, metadata->id );

    // Member variables
    for( size_t i = 0; i < metadata->memberVariables.size(); i++ )
    {
        if( nodeData->variables[i].i != metadata->memberVariables[i].valueDefault.i )
        {
            AddMemberLookupToDataStream( dataStream, 0, (uint8_t)i );

            AddToDataStream( dataStream, nodeData->variables[i].i );
        }
    }

    if( metadata->memberNodeLookups.size() )
    {
        AddMemberLookupToDataStream( dataStream, 1, (uint8_t)metadata->memberNodeLookups.size() );
    }

    // Member nodes
    for( size_t i = 0; i < metadata->memberNodeLookups.size(); i++ )
    {
        if( fixUp && nodeData->nodeLookups[i] )
        {
            // Create test node to see if source is a valid node type
            SmartNode<> test = metadata->CreateNode();
            SmartNode<> node = nodeData->nodeLookups[i]->metadata->CreateNode();

            if( !metadata->memberNodeLookups[i].setFunc( test.get(), node ) )
            {
                nodeData->nodeLookups[i] = nullptr;
                return false;
            }
        }

        if( !nodeData->nodeLookups[i] || !SerialiseNodeDataInternal( nodeData->nodeLookups[i], fixUp, dataStream, referenceIds, dependencies ) )
        {
            return false;
        }
    }

    // Member hybrids
    for( size_t i = 0; i < metadata->memberHybrids.size(); i++ )
    {
        if( !nodeData->hybrids[i].first )
        {
            if( nodeData->hybrids[i].second != metadata->memberHybrids[i].valueDefault )
            {
                AddMemberLookupToDataStream( dataStream, 2, (uint8_t)i );

                Metadata::MemberVariable::ValueUnion v = nodeData->hybrids[i].second;

                AddToDataStream( dataStream, v.i );
            }
        }
        else
        {
            if( fixUp )
            {
                // Create test node to see if source is a valid node type
                SmartNode<> test = metadata->CreateNode();
                SmartNode<> node = nodeData->hybrids[i].first->metadata->CreateNode();

                if( !metadata->memberHybrids[i].setNodeFunc( test.get(), node ) )
                {
                    nodeData->hybrids[i].first = nullptr;
                    return false;
                }
            }

            AddMemberLookupToDataStream( dataStream, 3, (uint8_t)i );

            if( !SerialiseNodeDataInternal( nodeData->hybrids[i].first, fixUp, dataStream, referenceIds, dependencies ) )
            {
                return false;
            }
        }
    }

    // Mark end of node
    AddToDataStream( dataStream, (uint8_t)255 );

    referenceIds.emplace( nodeData, (uint16_t)referenceIds.size() );

    return true;
}

std::string Metadata::SerialiseNodeData( NodeData* nodeData, bool fixUp )
{
    std::vector<uint8_t> serialData;
    std::unordered_map<const NodeData*, uint16_t> referenceIds;

    if( !SerialiseNodeDataInternal( nodeData, fixUp, serialData, referenceIds ) )
    {
        return "";
    }
    return Base64::Encode( serialData );
}

template<typename T>
static bool GetFromDataStream( const std::vector<uint8_t>& dataStream, size_t& idx, T& value )
{
    if( dataStream.size() < idx + sizeof( T ) )
    {
        return false;
    }

    value = *reinterpret_cast<const T*>( dataStream.data() + idx );

    idx += sizeof( T );
    return true;
}

static SmartNode<> DeserialiseSmartNodeInternal( const std::vector<uint8_t>& serialisedNodeData, size_t& serialIdx, std::vector<SmartNode<>>& referenceNodes, FastSIMD::FeatureSet level = FastSIMD::FeatureSet::Max )
{
    Metadata::node_id nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    // UINT8_MAX indicates a reference node
    if( nodeId == std::numeric_limits<Metadata::node_id>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        if( referenceId >= referenceNodes.size() )
        {
            return nullptr;            
        }

        return referenceNodes[referenceId];
    }

    // Create node from nodeId
    const Metadata* metadata = Metadata::GetFromId( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    SmartNode<> generator = metadata->CreateNode( level );

    if( !generator )
    {
        return nullptr;
    }

    MemberLookup memberLookup;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
    {
        return nullptr;
    }

    // Member variables
    while( memberLookup.member.type == 0 )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, serialIdx, v.i ) )
        {
            return nullptr;
        }

        if( memberLookup.member.index < metadata->memberVariables.size() )
        {
            metadata->memberVariables[memberLookup.member.index].setFunc( generator.get(), v );
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member nodes
    if( memberLookup.member.type == 1 )
    {
        size_t i = 0;
        for( ; i < std::min<size_t>( memberLookup.member.index, metadata->memberNodeLookups.size() ); i++ )
        {
            SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level );

            if( !nodeGen || !metadata->memberNodeLookups[i].setFunc( generator.get(), nodeGen ) )
            {
                return nullptr;
            }
        }
        for( ; i < memberLookup.member.index; i++ )
        {
            // Still need to deserialise this even if there is no where to put it
            if( !DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level ) )
            {
                return nullptr;
            }
        }
        for( ; i < metadata->memberNodeLookups.size(); i++ )
        {
            // Attempt to use a dummy node to fill the new node lookup
            if( !metadata->memberNodeLookups[i].setFunc( generator.get(), FastNoise::New<FastNoise::Constant>( level ) ) )
            {
                return nullptr;
            }
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member hybrids
    while( memberLookup.data != 255 )
    {
        if( memberLookup.member.type == 3 )
        {
            SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level );

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                if( !nodeGen || !metadata->memberHybrids[memberLookup.member.index].setNodeFunc( generator.get(), nodeGen ) )
                {
                    return nullptr;
                }
            }
        }
        else
        {
            float v;

            if( !GetFromDataStream( serialisedNodeData, serialIdx, v ) )
            {
                return nullptr;
            }

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                metadata->memberHybrids[memberLookup.member.index].setValueFunc( generator.get(), v );
            }
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    referenceNodes.emplace_back( generator );

    return generator;
}

SmartNode<> FastNoise::NewFromEncodedNodeTree( const char* serialisedBase64NodeData, FastSIMD::FeatureSet level )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    std::vector<SmartNode<>> referenceNodes;

    return DeserialiseSmartNodeInternal( dataStream, startIdx, referenceNodes, level );
}

static NodeData* DeserialiseNodeDataInternal( const std::vector<uint8_t>& serialisedNodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut, size_t& serialIdx )
{
    Metadata::node_id nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    // UINT8_MAX indicates a reference node
    if( nodeId == std::numeric_limits<Metadata::node_id>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        if( referenceId >= nodeDataOut.size() )
        {
            return nullptr;
        }

        return nodeDataOut[referenceId].get();
    }

    // Create node from nodeId
    const Metadata* metadata = Metadata::GetFromId( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    std::unique_ptr<NodeData> nodeData( new NodeData( metadata ) );


    MemberLookup memberLookup;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
    {
        return nullptr;
    }

    // Member variables
    while( memberLookup.member.type == 0 )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, serialIdx, v.i ) )
        {
            return nullptr;
        }

        if( memberLookup.member.index < metadata->memberVariables.size() )
        {
            nodeData->variables[memberLookup.member.index] = v;
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member nodes
    if( memberLookup.member.type == 1 )
    {
        size_t i = 0;
        for( ; i < std::min<size_t>( memberLookup.member.index, metadata->memberNodeLookups.size() ); i++ )
        {
            nodeData->nodeLookups[i] = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx );
        }
        for( ; i < memberLookup.member.index; i++ )
        {
            // Still need to deserialise this even if there is no where to put it
            DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx );
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member hybrids
    while( memberLookup.data != 255 )
    {
        if( memberLookup.member.type == 3 )
        {
            NodeData* node = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx );

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                nodeData->hybrids[memberLookup.member.index].first = node;
            }
        }
        else
        {
            float v;

            if( !GetFromDataStream( serialisedNodeData, serialIdx, v ) )
            {
                return nullptr;
            }

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                nodeData->hybrids[memberLookup.member.index].second = v;
            }
        }

        if( !GetFromDataStream( serialisedNodeData, serialIdx, memberLookup ) )
        {
            return nullptr;
        }
    }

    return nodeDataOut.emplace_back( std::move( nodeData ) ).get();  
}

NodeData* Metadata::DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    return DeserialiseNodeDataInternal( dataStream, nodeDataOut, startIdx );
}

std::string Metadata::FormatMetadataNodeName( const Metadata* metadata, bool removeGroups )
{
    std::string string;

    if( metadata->formattedName )
    {
        string = metadata->formattedName;
    }
    else
    {
        string = metadata->name;
        for( size_t i = 1; i < string.size(); i++ )
        {
            if( ( isdigit( string[i] ) || isupper( string[i] ) ) && islower( string[i - 1] ) )
            {
                string.insert( i++, 1, ' ' );
            }
        }
    }

    if( removeGroups )
    {
        for( auto group : metadata->groups )
        {
            size_t start_pos = string.find( group );
            if( start_pos != std::string::npos )
            {
                string.erase( start_pos, std::strlen( group ) + 1 );
            }
        }
    }

    // Fallback since empty strings cause imgui errors
    if( string.empty() )
    {
        return metadata->name;
    }

    return string;
}

std::string Metadata::FormatMetadataMemberName( const Member& member )
{
    std::string string = member.name;
    if( member.dimensionIdx >= 0 )
    {
        string.insert( string.begin(), ' ' );
        string.insert( 0, kDim_Strings[member.dimensionIdx] );
    }
    return string;
}

namespace FastNoise
{
    template<typename T>
    struct MetadataT;
}

template<typename T>
static std::unique_ptr<const MetadataT<T>> CreateMetadataInstance( const char* className )
{
    auto* newMetadata = new MetadataT<T>;
    newMetadata->name = className;

    // Node must be in a group or it is not selectable in the UI
    assert( newMetadata->groups.size() );
    return std::unique_ptr<const MetadataT<T>>( newMetadata );
}

#define FASTNOISE_REGISTER_NODE( CLASS ) \
static const std::unique_ptr<const FastNoise::MetadataT<CLASS>> g ## CLASS ## Metadata = CreateMetadataInstance<CLASS>( #CLASS );\
template<> FASTNOISE_API const FastNoise::Metadata& FastNoise::Impl::GetMetadata<CLASS>()\
{\
    return *g ## CLASS ## Metadata;\
}\
const FastNoise::Metadata& CLASS::GetMetadata() const\
{\
    return FastNoise::Impl::GetMetadata<CLASS>();\
}\
SmartNode<> FastNoise::MetadataT<CLASS>::CreateNode( FastSIMD::FeatureSet l ) const\
{\
    return SmartNode<>( FastSIMD::NewDispatchClass<CLASS>( l, &SmartNodeManager::Allocate ) );\
}

#define FASTSIMD_INCLUDE_HEADER_ONLY
#include "FastSIMD_Build.inl"