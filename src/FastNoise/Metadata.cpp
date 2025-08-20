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
constexpr size_t gMetadataVectorSize<const Metadata*> = 47;
template<>
constexpr size_t gMetadataVectorSize<const char*> = 91;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberVariable> = 72;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberNodeLookup> = 32;
template<>
constexpr size_t gMetadataVectorSize<Metadata::MemberHybrid> = 60;

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
    enum Type : uint8_t
    {
        TypeVariable,
        TypeLookup,
        TypeHybridLookup,
        TypeHybridVariable,
        NodeEnd,
    };

    struct
    {
        uint8_t type : 3;
        uint8_t index : 5;
    } member;

    uint8_t data;
};

struct DataStream
{
    std::vector<uint8_t> stream;
    mutable int nodeEndStack = 0;
    mutable int streamPos = 0;
};

template<typename T>
static void AddToDataStream( DataStream& , T );

static void DataStreamCheckNodeEnd( DataStream& dataStream )
{
    if( dataStream.nodeEndStack > 0 )
    {
        MemberLookup memberLookup;
        memberLookup.member.type = MemberLookup::NodeEnd;
        memberLookup.member.index = (uint8_t)dataStream.nodeEndStack - 1;
        dataStream.nodeEndStack = 0;

        AddToDataStream( dataStream, memberLookup.data );
    }
}

template<typename T>
static void AddToDataStream( DataStream& dataStream, T value )
{
    DataStreamCheckNodeEnd( dataStream );

    for( size_t i = 0; i < sizeof( T ); i++ )
    {
        dataStream.stream.push_back( (uint8_t)(value >> (i * 8)) );
    }
}

static void AddMemberLookupToDataStream( DataStream& dataStream, MemberLookup::Type type, uint8_t index )
{
    MemberLookup memberLookup;
    memberLookup.member.type = type;
    memberLookup.member.index = index;

    AddToDataStream( dataStream, memberLookup.data );
}

static void AddNodeEndToDataStream( DataStream& dataStream )
{
    if( ++dataStream.nodeEndStack < 32 )
    {
        return;
    }
    DataStreamCheckNodeEnd( dataStream );
}


static bool SerialiseNodeDataInternal( NodeData* nodeData, bool fixUp, DataStream& dataStream, std::unordered_map<const NodeData*, uint16_t>& referenceIds, std::unordered_set<const NodeData*> dependencies = {} )
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
            AddMemberLookupToDataStream( dataStream, MemberLookup::TypeVariable, (uint8_t)i );

            AddToDataStream( dataStream, nodeData->variables[i].i );
        }
    }

    if( metadata->memberNodeLookups.size() )
    {
        AddMemberLookupToDataStream( dataStream, MemberLookup::TypeLookup, (uint8_t)metadata->memberNodeLookups.size() );
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
                AddMemberLookupToDataStream( dataStream, MemberLookup::TypeHybridVariable, (uint8_t)i );

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

            AddMemberLookupToDataStream( dataStream, MemberLookup::TypeHybridLookup, (uint8_t)i );

            if( !SerialiseNodeDataInternal( nodeData->hybrids[i].first, fixUp, dataStream, referenceIds, dependencies ) )
            {
                return false;
            }
        }
    }

    // Mark end of node
    AddNodeEndToDataStream( dataStream );

    referenceIds.emplace( nodeData, (uint16_t)referenceIds.size() );

    return true;
}

std::string Metadata::SerialiseNodeData( NodeData* nodeData, bool fixUp )
{
    DataStream dataStream;
    std::unordered_map<const NodeData*, uint16_t> referenceIds;

    if( !SerialiseNodeDataInternal( nodeData, fixUp, dataStream, referenceIds ) )
    {
        return "";
    }
    DataStreamCheckNodeEnd( dataStream );
    return Base64::Encode( dataStream.stream );
}

template<typename T>
static bool GetFromDataStream( const DataStream& dataStream, T& value )
{
    if( dataStream.nodeEndStack > 0 )
    {
        return false;
    }

    if( dataStream.stream.size() < dataStream.streamPos + sizeof( T ) )
    {
        return false;
    }

    value = *reinterpret_cast<const T*>( dataStream.stream.data() + dataStream.streamPos );

    dataStream.streamPos += sizeof( T );
    return true;
}

template<>
bool GetFromDataStream<MemberLookup>( const DataStream& dataStream, MemberLookup& value )
{
    if( dataStream.nodeEndStack > 0 )
    {
        --dataStream.nodeEndStack;
        value.member.type = MemberLookup::NodeEnd;
        return true;
    }

    if( dataStream.stream.size() < dataStream.streamPos + sizeof( MemberLookup ) )
    {
        return false;
    }

    value = *reinterpret_cast<const MemberLookup*>( dataStream.stream.data() + dataStream.streamPos );

    dataStream.streamPos += sizeof( MemberLookup );

    if( value.member.type == MemberLookup::NodeEnd )
    {
        dataStream.nodeEndStack = value.member.index;
    }
    return true;
}

static SmartNode<> DeserialiseSmartNodeInternal( const DataStream& serialisedNodeData, std::vector<SmartNode<>>& referenceNodes, FastSIMD::FeatureSet level = FastSIMD::FeatureSet::Max )
{
    Metadata::node_id nodeId;
    if( !GetFromDataStream( serialisedNodeData, nodeId ) )
    {
        return nullptr;
    }

    // UINT8_MAX indicates a reference node
    if( nodeId == std::numeric_limits<Metadata::node_id>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, referenceId ) )
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
    if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
    {
        return nullptr;
    }

    // Member variables
    while( memberLookup.member.type == MemberLookup::TypeVariable )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, v.i ) )
        {
            return nullptr;
        }

        if( memberLookup.member.index < metadata->memberVariables.size() )
        {
            metadata->memberVariables[memberLookup.member.index].setFunc( generator.get(), v );
        }

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member nodes
    if( memberLookup.member.type == MemberLookup::TypeLookup )
    {
        size_t i = 0;
        for( ; i < std::min<size_t>( memberLookup.member.index, metadata->memberNodeLookups.size() ); i++ )
        {
            SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, referenceNodes, level );

            if( !nodeGen || !metadata->memberNodeLookups[i].setFunc( generator.get(), nodeGen ) )
            {
                return nullptr;
            }
        }
        for( ; i < memberLookup.member.index; i++ )
        {
            // Still need to deserialise this even if there is no where to put it
            if( !DeserialiseSmartNodeInternal( serialisedNodeData, referenceNodes, level ) )
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

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member hybrids
    while( memberLookup.member.type != MemberLookup::NodeEnd )
    {
        if( memberLookup.member.type == MemberLookup::TypeHybridLookup )
        {
            SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, referenceNodes, level );

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                if( !nodeGen || !metadata->memberHybrids[memberLookup.member.index].setNodeFunc( generator.get(), nodeGen ) )
                {
                    return nullptr;
                }
            }
        }
        else if( memberLookup.member.type == MemberLookup::TypeHybridVariable )
        {
            float v;

            if( !GetFromDataStream( serialisedNodeData, v ) )
            {
                return nullptr;
            }

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                metadata->memberHybrids[memberLookup.member.index].setValueFunc( generator.get(), v );
            }
        }

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    referenceNodes.emplace_back( generator );

    return generator;
}

SmartNode<> FastNoise::NewFromEncodedNodeTree( const char* serialisedBase64NodeData, FastSIMD::FeatureSet level )
{
    DataStream dataStream = { Base64::Decode( serialisedBase64NodeData ) };

    std::vector<SmartNode<>> referenceNodes;

    return DeserialiseSmartNodeInternal( dataStream, referenceNodes, level );
}

static NodeData* DeserialiseNodeDataInternal( const DataStream& serialisedNodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut )
{
    Metadata::node_id nodeId;
    if( !GetFromDataStream( serialisedNodeData, nodeId ) )
    {
        return nullptr;
    }

    // UINT8_MAX indicates a reference node
    if( nodeId == std::numeric_limits<Metadata::node_id>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, referenceId ) )
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
    if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
    {
        return nullptr;
    }

    // Member variables
    while( memberLookup.member.type == MemberLookup::TypeVariable )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, v.i ) )
        {
            return nullptr;
        }

        if( memberLookup.member.index < metadata->memberVariables.size() )
        {
            nodeData->variables[memberLookup.member.index] = v;
        }

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member nodes
    if( memberLookup.member.type == MemberLookup::TypeLookup )
    {
        size_t i = 0;
        for( ; i < std::min<size_t>( memberLookup.member.index, metadata->memberNodeLookups.size() ); i++ )
        {
            nodeData->nodeLookups[i] = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut );
        }
        for( ; i < memberLookup.member.index; i++ )
        {
            // Still need to deserialise this even if there is no where to put it
            DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut );
        }

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    // Member hybrids
    while( memberLookup.member.type != MemberLookup::NodeEnd )
    {
        if( memberLookup.member.type == MemberLookup::TypeHybridLookup )
        {
            NodeData* node = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut );

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                nodeData->hybrids[memberLookup.member.index].first = node;
            }
        }
        else if( memberLookup.member.type == MemberLookup::TypeHybridVariable )
        {
            float v;

            if( !GetFromDataStream( serialisedNodeData, v ) )
            {
                return nullptr;
            }

            if( memberLookup.member.index < metadata->memberHybrids.size() )
            {
                nodeData->hybrids[memberLookup.member.index].second = v;
            }
        }

        if( !GetFromDataStream( serialisedNodeData, memberLookup ) )
        {
            return nullptr;
        }
    }

    return nodeDataOut.emplace_back( std::move( nodeData ) ).get();  
}

NodeData* Metadata::DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut )
{
    DataStream dataStream = { Base64::Decode( serialisedBase64NodeData ) };

    return DeserialiseNodeDataInternal( dataStream, nodeDataOut );
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