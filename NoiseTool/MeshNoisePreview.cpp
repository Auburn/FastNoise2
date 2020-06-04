#include "MeshNoisePreview.h"

#include <imgui.h>

#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/Phong.h>

using namespace Magnum;

MeshNoisePreview::MeshNoisePreview()
{
    for( int i = 0; i < 8; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }
}

void MeshNoisePreview::ReGenerate( const std::shared_ptr<FastNoise::Generator>& generator, float frequency, int32_t seed )
{
    mBuildData.generator = generator;
    mBuildData.frequency = frequency;
    mBuildData.seed = seed;
    mBuildData.isoSurface = 0.0f;
    mBuildData.pos = Vector3i( 0 );
    mBuildData.genVersion = mCompleteQueue.IncVersion();

    mChunks.clear();
    mGenerateQueue.Clear();

    Chunk::MeshData meshData;
    while( mCompleteQueue.Pop( meshData ) )
    {
        meshData.Free();
    }

    const int range = 2;

    for( int x = -range; x <= range; x++ )
    {
        for( int y = -range; y <= range; y++ )
        {
            for( int z = -range; z <= range; z++ )
            {
                mBuildData.pos = Vector3i( x, y, z ) * Chunk::SIZE;
                mGenerateQueue.Push( mBuildData );
            }
        }
    }

}

void MeshNoisePreview::Draw( const Matrix4& transformation, const Matrix4& projection )
{
    Chunk::MeshData meshData;
    if( mCompleteQueue.Pop( meshData ) )
    {        
        mChunks.emplace_back( meshData );
    }

    mShader.setLightPosition( { 10000.0f, 6000.0f, 8000.0f } )
           .setLightColor( Color3{ 0.6f } )
           .setAmbientColor( Color3( 0.2f ) )
           .setTransformationMatrix( transformation )
           .setNormalMatrix( transformation.rotationScaling() )
           .setProjectionMatrix( projection );

    size_t triCount = 0;

    for( Chunk& chunk : mChunks )
    {
        triCount += chunk.GetMesh().count() / 3;
        chunk.GetMesh().draw( mShader );
    }

    ImGui::Text( "Triangle Count: %0.1fK", triCount / 1000.0 );
}

void MeshNoisePreview::GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue  )
{
    while( true )
    {
        Chunk::BuildData buildData = generateQueue.Pop();

        Chunk::MeshData meshData = Chunk::BuildMeshData( buildData );

        if( !completeQueue.Push( meshData, buildData.genVersion ) )
        {
            meshData.Free();
        }
    }
}


MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildMeshData( const BuildData& buildData )
{
    thread_local static float densityValues[SIZE_GEN * SIZE_GEN * SIZE_GEN];
    thread_local static std::vector<VertexData> vertexData;
    thread_local static std::vector<uint32_t> indicies;

    buildData.generator->GenUniformGrid3D( densityValues, 
        buildData.pos.x() - 1, buildData.pos.y() - 1, buildData.pos.z() - 1,
        SIZE_GEN, SIZE_GEN, SIZE_GEN, buildData.frequency, buildData.seed );

    vertexData.clear();
    indicies.clear();

    const int32_t STEP_X = SIZE_GEN * SIZE_GEN;
    const int32_t STEP_Y = SIZE_GEN;
    const int32_t STEP_Z = 1;

    int32_t noiseIdx = STEP_X + STEP_Y + STEP_Z;

    for( uint32_t x = 0; x < SIZE; x++ )
    {
        float xf = x + (float)buildData.pos.x();

        for( uint32_t y = 0; y < SIZE; y++ )
        {
            float yf = y + (float)buildData.pos.y();

            for( uint32_t z = 0; z < SIZE; z++ )
            {
                float zf = z + (float)buildData.pos.z();

                if( densityValues[noiseIdx] >= buildData.isoSurface ) // isSolid
                {
                    if( densityValues[noiseIdx + STEP_X] < buildData.isoSurface ) // Right
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_X, STEP_Y, STEP_Z, Vector3(1,0,0),
                                 Vector3(xf + 1,yf,zf), Vector3(xf + 1,yf + 1,zf), Vector3(xf + 1,yf + 1,zf + 1), Vector3(xf + 1,yf,zf + 1) );
                    }
                    
                    if( densityValues[noiseIdx - STEP_X] < buildData.isoSurface ) // Left
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_X, -STEP_Y, STEP_Z, Vector3(-1,0,0),
                                 Vector3(xf,yf + 1,zf), Vector3(xf,yf,zf), Vector3(xf,yf,zf + 1), Vector3(xf,yf + 1,zf + 1) );
                    }

                    if( densityValues[noiseIdx + STEP_Y] < buildData.isoSurface ) // Up
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_Y, -STEP_X, STEP_Z, Vector3(0,1,0),
                                 Vector3(xf + 1,yf + 1,zf),Vector3(xf,yf + 1,zf), Vector3(xf,yf + 1,zf + 1), Vector3(xf + 1,yf + 1,zf + 1) );
                    }
                    
                    if( densityValues[noiseIdx - STEP_Y] < buildData.isoSurface ) // Down
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_Y, STEP_X, STEP_Z, Vector3(0,-1,0),
                                 Vector3(xf,yf,zf), Vector3(xf + 1,yf,zf), Vector3(xf + 1,yf,zf + 1), Vector3(xf,yf,zf + 1) );
                    }

                    if( densityValues[noiseIdx + STEP_Z] < buildData.isoSurface ) // Forward
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_Z, STEP_X, STEP_Y, Vector3(0,0,1),
                                  Vector3(xf,yf,zf + 1), Vector3(xf + 1,yf,zf + 1), Vector3(xf + 1,yf + 1,zf + 1), Vector3(xf,yf + 1,zf + 1) );
                    }
                    
                    if( densityValues[noiseIdx - STEP_Z] < buildData.isoSurface ) // Back
                    {
                        AddQuad( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_Z, -STEP_X, STEP_Y, Vector3(0,0,-1),
                                 Vector3(xf + 1,yf,zf), Vector3(xf,yf,zf), Vector3(xf,yf + 1,zf), Vector3(xf + 1,yf + 1,zf ) );
                    }
                }
                noiseIdx++;
            }

            noiseIdx += STEP_Z * 2;
        }
        
        noiseIdx += STEP_Y * 2;
    }

    MeshData meshData( buildData.pos, vertexData, indicies );

    return meshData;    
}

MeshNoisePreview::Chunk::Chunk( MeshData& meshData )
{
    mVertexBuffer.setData( meshData.vertexData );
    mIndexBuffer.setData( meshData.indicies );

    //https://doc.magnum.graphics/magnum/classMagnum_1_1GL_1_1Mesh.html

    mMesh.setPrimitive( GL::MeshPrimitive::Triangles )
        .setCount( mIndexBuffer.size() )
        .setIndexBuffer( mIndexBuffer, 0, GL::MeshIndexType::UnsignedInt, 0, mVertexBuffer.size() - 1 )
        .addVertexBuffer( mVertexBuffer, 0, Shaders::Phong::Position{}, Shaders::Phong::Normal{}, Shaders::Phong::Color3{} );

    meshData.Free();
}

void MeshNoisePreview::Chunk::AddQuad( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
    int32_t facingIdx, int32_t offsetA, int32_t offsetB, Vector3 normal, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 )
{
    uint8_t sideA0 = density[facingIdx - offsetA] >= isoSurface;
    uint8_t sideA1 = density[facingIdx + offsetA] >= isoSurface;
    uint8_t sideB0 = density[facingIdx - offsetB] >= isoSurface;
    uint8_t sideB1 = density[facingIdx + offsetB] >= isoSurface;

    uint8_t corner00 = (sideA0 && sideB0) || density[facingIdx - offsetA - offsetB] >= isoSurface;
    uint8_t corner01 = (sideA0 && sideB1) || density[facingIdx - offsetA + offsetB] >= isoSurface;
    uint8_t corner10 = (sideA1 && sideB0) || density[facingIdx + offsetA - offsetB] >= isoSurface;
    uint8_t corner11 = (sideA1 && sideB1) || density[facingIdx + offsetA + offsetB] >= isoSurface;

    uint8_t light00 = 3 - (sideA0 + sideB0 + corner00);
    uint8_t light01 = 3 - (sideA1 + sideB0 + corner10);
    uint8_t light10 = 3 - (sideA0 + sideB1 + corner01);
    uint8_t light11 = 3 - (sideA1 + sideB1 + corner11);

    uint32_t vertIdx = (uint32_t)verts.size();
    verts.emplace_back( pos00, normal, Color3( ( (float)light00 / 3.0f ) * AO_STRENGTH ) );
    verts.emplace_back( pos01, normal, Color3( ( (float)light01 / 3.0f ) * AO_STRENGTH ) );
    verts.emplace_back( pos10, normal, Color3( ( (float)light10 / 3.0f ) * AO_STRENGTH ) );
    verts.emplace_back( pos11, normal, Color3( ( (float)light11 / 3.0f ) * AO_STRENGTH ) );

    if( light00 + light11 > light01 + light10 )
    {        
        indicies.push_back( vertIdx );
        indicies.push_back( vertIdx + 3 );
        indicies.push_back( vertIdx + 2 );
        indicies.push_back( vertIdx + 3 );
        indicies.push_back( vertIdx );
        indicies.push_back( vertIdx + 1 );    
    }
    else
    {        
        indicies.push_back( vertIdx );
        indicies.push_back( vertIdx + 1 );
        indicies.push_back( vertIdx + 2 );
        indicies.push_back( vertIdx + 3 );
        indicies.push_back( vertIdx + 2 );
        indicies.push_back( vertIdx + 1 );
    }
}
