#include <FastNoise/FastNoise_C.h>
#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>

namespace FastNoise::Internal
{
    // Manually bump the reference count on a raw Generator pointer. Avoids
    // needing to keep a SmartNode around for nodes created in the C API
    void BumpNodeRefences( const Generator* ptr, bool up )
    {
        ptr->ReferencesFetchAdd( up ? 1 : -1 );
    }

    // Setting node sources requires a SmartNode argument, this SmartNode friend function
    // allows the C API to convert raw pointers to temporary SmartNodes for this purpose
    SmartNode<> ToSmartNode( const void* p )
    {
        return SmartNode{ static_cast<Generator*>( const_cast<void*>( p ) ) };
    }
}

FastNoise::Generator* ToGen( void* p )
{
    return static_cast<FastNoise::Generator*>( p );
}

const FastNoise::Generator* ToGen( const void* p )
{
    return static_cast<const FastNoise::Generator*>( p );
}

void StoreMinMax( float* floatArray2, FastNoise::OutputMinMax minMax )
{
    if( floatArray2 )
    {
        floatArray2[0] = minMax.min;
        floatArray2[1] = minMax.max;
    }
}

void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned simdLevel )
{
    if( FastNoise::SmartNode<> node = FastNoise::NewFromEncodedNodeTree( encodedString, (FastSIMD::FeatureSet)simdLevel ) )
    {
        FastNoise::Internal::BumpNodeRefences( node.get(), true );

        return node.get();
    }
    return nullptr;
}

void fnDeleteNodeRef( void* node )
{
    FastNoise::Internal::BumpNodeRefences( ToGen( node ), false );
}

unsigned fnGetSIMDLevel( const void* node )
{
    return (unsigned)ToGen( node )->GetActiveFeatureSet();
}

int fnGetMetadataID( const void* node )
{
    return ToGen( node )->GetMetadata().id;
}

void fnGenUniformGrid2D( const void* node, float* noiseOut, float xOffset, float yOffset, int xCount, int yCount, float xStepSize, float yStepSize, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid2D( noiseOut, xOffset, yOffset, xCount, yCount, xStepSize, yStepSize, seed ) );    
}

void fnGenUniformGrid3D( const void* node, float* noiseOut, float xOffset, float yOffset, float zOffset, int xCount, int yCount, int zCount, float xStepSize, float yStepSize, float zStepSize, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid3D( noiseOut, xOffset, yOffset, zOffset, xCount, yCount, zCount, xStepSize, yStepSize, zStepSize, seed ) );    
}

void fnGenUniformGrid4D( const void* node, float* noiseOut, float xOffset, float yOffset, float zOffset, float wOffset, int xCount, int yCount, int zCount, int wCount, float xStepSize, float yStepSize, float zStepSize, float wStepSize, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid4D( noiseOut, xOffset, yOffset, zOffset, wOffset, xCount, yCount, zCount, wCount, xStepSize, yStepSize, zStepSize, wStepSize, seed ) );    
}

void fnGenPositionArray2D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray2D( noiseOut, count, xPosArray, yPosArray, xOffset, yOffset, seed ) );
}

void fnGenPositionArray3D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray3D( noiseOut, count, xPosArray, yPosArray, zPosArray, xOffset, yOffset, zOffset, seed ) );
}

void fnGenPositionArray4D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray4D( noiseOut, count, xPosArray, yPosArray, zPosArray, wPosArray, xOffset, yOffset, zOffset, wOffset, seed ) );
}

float fnGenSingle2D( const void* node, float x, float y, int seed )
{
    return ToGen( node )->GenSingle2D( x, y, seed );
}

float fnGenSingle3D( const void* node, float x, float y, float z, int seed )
{
    return ToGen( node )->GenSingle3D( x, y, z, seed );
}

float fnGenSingle4D( const void* node, float x, float y, float z, float w, int seed )
{
    return ToGen( node )->GenSingle4D( x, y, z, w, seed );
}

void fnGenTileable2D( const void* node, float* noiseOut, int xSize, int ySize, float xStepSize, float yStepSize, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenTileable2D( noiseOut, xSize, ySize, xStepSize, yStepSize, seed ) );
}

int fnGetMetadataCount()
{
    return (int)FastNoise::Metadata::GetAll().size();
}

const char* fnGetMetadataName( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        return metadata->name;
    }
    return "INVALID NODE ID";
}

void* fnNewFromMetadata( int id, unsigned simdLevel )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        FastNoise::SmartNode<> node = metadata->CreateNode( (FastSIMD::FeatureSet)simdLevel );
        FastNoise::Internal::BumpNodeRefences( node.get(), true );

        return node.get();
    }
    return nullptr;
}

int fnGetMetadataVariableCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        return (int)metadata->memberVariables.size();
    }
    return -1;
}

const char* fnGetMetadataVariableName( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return metadata->memberVariables[variableIndex].name;
        }
        return "INVALID VARIABLE INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataVariableType( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return (int)metadata->memberVariables[variableIndex].type;
        }
        return -1;
    }
    return -1;
}

int fnGetMetadataVariableDimensionIdx( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return metadata->memberVariables[variableIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

int fnGetMetadataEnumCount( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return (int)metadata->memberVariables[variableIndex].enumNames.size();
        }
        return -1;
    }
    return -1;
}

const char* fnGetMetadataEnumName( int id, int variableIndex, int enumIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            if( (size_t)enumIndex < metadata->memberVariables[variableIndex].enumNames.size() )
            {
                return metadata->memberVariables[variableIndex].enumNames[enumIndex];
            }
            return "INVALID ENUM INDEX";
        }
        return "INVALID VARIABLE INDEX";
    }
    return "INVALID NODE ID";
}

bool fnSetVariableFloat( void* node, int variableIndex, float value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)variableIndex < metadata.memberVariables.size() )
    {
        return metadata.memberVariables[variableIndex].setFunc( ToGen( node ), value );
    }
    return false;
}

bool fnSetVariableIntEnum( void* node, int variableIndex, int value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)variableIndex < metadata.memberVariables.size() )
    {
        return metadata.memberVariables[variableIndex].setFunc( ToGen( node ), value );
    }
    return false;
}

int fnGetMetadataNodeLookupCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        return (int)metadata->memberNodeLookups.size();
    }
    return -1;
}

const char* fnGetMetadataNodeLookupName( int id, int nodeLookupIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)nodeLookupIndex < metadata->memberNodeLookups.size() )
        {
            return metadata->memberNodeLookups[nodeLookupIndex].name;
        }
        return "INVALID NODE LOOKUP INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataNodeLookupDimensionIdx( int id, int nodeLookupIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)nodeLookupIndex < metadata->memberNodeLookups.size() )
        {
            return metadata->memberNodeLookups[nodeLookupIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

bool fnSetNodeLookup( void* node, int nodeLookupIndex, const void* nodeLookup )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)nodeLookupIndex < metadata.memberNodeLookups.size() )
    {
        FastNoise::SmartNode<> smartLookup = FastNoise::Internal::ToSmartNode( nodeLookup );
        return metadata.memberNodeLookups[nodeLookupIndex].setFunc( ToGen( node ), smartLookup );
    }
    return false;
}

int fnGetMetadataHybridCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        return (int)metadata->memberHybrids.size();
    }
    return -1;
}

const char* fnGetMetadataHybridName( int id, int hybridIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)hybridIndex < metadata->memberHybrids.size() )
        {
            return metadata->memberHybrids[hybridIndex].name;
        }
        return "INVALID HYBRID INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataHybridDimensionIdx( int id, int hybridIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
    {
        if( (size_t)hybridIndex < metadata->memberHybrids.size() )
        {
            return metadata->memberHybrids[hybridIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

bool fnSetHybridNodeLookup( void* node, int hybridIndex, const void* nodeLookup )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)hybridIndex < metadata.memberHybrids.size() )
    {
        FastNoise::SmartNode<> smartLookup = FastNoise::Internal::ToSmartNode( nodeLookup );
        return metadata.memberHybrids[hybridIndex].setNodeFunc( ToGen( node ), smartLookup );
    }
    return false;
}

bool fnSetHybridFloat( void* node, int hybridIndex, float value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)hybridIndex < metadata.memberHybrids.size() )
    {
        return metadata.memberHybrids[hybridIndex].setValueFunc( ToGen( node ), value );
    }
    return false;
}

const char* fnGetMetadataDescription( int id )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        return m->description;
    return "";
}

int fnGetMetadataGroupCount( int id )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        return (int)m->groups.size();
    return 0;
}

const char* fnGetMetadataGroupName( int id, int groupIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)groupIndex < m->groups.size() )
            return m->groups[groupIndex];
    return "";
}

const char* fnGetMetadataVariableDescription( int id, int variableIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)variableIndex < m->memberVariables.size() )
            return m->memberVariables[variableIndex].description;
    return "";
}

float fnGetMetadataVariableDefaultFloat( int id, int variableIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)variableIndex < m->memberVariables.size() )
            return m->memberVariables[variableIndex].valueDefault.f;
    return 0;
}

int fnGetMetadataVariableDefaultIntEnum( int id, int variableIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)variableIndex < m->memberVariables.size() )
            return m->memberVariables[variableIndex].valueDefault.i;
    return 0;
}

float fnGetMetadataVariableMinFloat( int id, int variableIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)variableIndex < m->memberVariables.size() )
            return m->memberVariables[variableIndex].valueMin.f;
    return 0;
}

float fnGetMetadataVariableMaxFloat( int id, int variableIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)variableIndex < m->memberVariables.size() )
            return m->memberVariables[variableIndex].valueMax.f;
    return 0;
}

const char* fnGetMetadataNodeLookupDescription( int id, int nodeLookupIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)nodeLookupIndex < m->memberNodeLookups.size() )
            return m->memberNodeLookups[nodeLookupIndex].description;
    return "";
}

const char* fnGetMetadataHybridDescription( int id, int hybridIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)hybridIndex < m->memberHybrids.size() )
            return m->memberHybrids[hybridIndex].description;
    return "";
}

float fnGetMetadataHybridDefault( int id, int hybridIndex )
{
    if( const auto* m = FastNoise::Metadata::GetFromId( (FastNoise::Metadata::node_id)id ) )
        if( (size_t)hybridIndex < m->memberHybrids.size() )
            return m->memberHybrids[hybridIndex].valueDefault;
    return 0;
}
