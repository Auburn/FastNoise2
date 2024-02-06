#include <algorithm>
#include <thread>
#include <cmath>

#include <Corrade/Utility/Resource.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Frustum.h>
#include <Magnum/Math/Intersection.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>

#include "ImGuiExtra.h"
#include "MeshNoisePreview.h"

using namespace Magnum;

MeshNoisePreview::MeshNoisePreview()
{
    mBuildData.frequency = 0.005f;
    mBuildData.seed = 1338;
    mBuildData.isoSurface = 0.0f;
    mBuildData.heightmapMultiplier = 100.0f;
    mBuildData.color = Color3( 1.0f );
    mBuildData.meshType = MeshType_Voxel3D;

    uint32_t threadCount = std::max( 2u, std::thread::hardware_concurrency() );

    threadCount -= threadCount / 4;

    for( uint32_t i = 0; i < threadCount; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }

    SetupSettingsHandlers();
}

MeshNoisePreview::~MeshNoisePreview()
{
    mGenerateQueue.KillThreads();

    for( auto& thread : mThreads )
    {
        thread.join();
    }
}

void MeshNoisePreview::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mLoadRange = 200.0f;
    mBuildData.generator = generator;
    mBuildData.pos = Vector3i( 0 );

    mMinMax = {};
    mMinAirY = INFINITY;
    mMaxSolidY = -INFINITY;

    mRegisteredChunkPositions.clear();
    mChunks.clear();
    mGenerateQueue.Clear();
    mBuildData.genVersion = mCompleteQueue.IncVersion();

    Chunk::MeshData meshData;
    while( mCompleteQueue.Pop( meshData ) )
    {
        meshData.Free();
    }
}

void MeshNoisePreview::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition )
{
    if( ImGui::Checkbox( "Generate Mesh Preview", &mEnabled ) )
    {
        ReGenerate( mBuildData.generator );    
        ImGuiExtra::MarkSettingsDirty();    
    }

    if( !mBuildData.generator || !mEnabled )
    {
        return;
    }

    UpdateChunkQueues( cameraPosition );

    Matrix4 transformationProjection = projection * transformation;

    Frustum camFrustum = Frustum::fromMatrix( transformationProjection );
    mShader.SetTransformationProjectionMatrix( transformationProjection );

    mTriCount = 0;
    mMeshesCount = 0;
    uint32_t drawnTriCount = 0;

    for( Chunk& chunk : mChunks )
    {
        if( GL::Mesh* mesh = chunk.GetMesh() )
        {
            int32_t meshTriCount = mesh->count();

            mTriCount += meshTriCount;
            mMeshesCount++;

            Vector3 posf( chunk.GetPos());
            Range3D bbox( posf, posf + Vector3( Chunk::SIZE + 1 ) );

            if( mBuildData.meshType == MeshType_Heightmap2D )
            {
                bbox.min().y() = mMinMax.min * mBuildData.heightmapMultiplier;
                bbox.max().y() = mMinMax.max * mBuildData.heightmapMultiplier;
            }

            if( Math::Intersection::rangeFrustum( bbox, camFrustum ) )
            {
                drawnTriCount += meshTriCount;
                mShader.draw( *mesh );
            }
        }
    }
    mTriCount /= 3;

    bool edited = false;
    edited |= ImGui::Combo( "Mesh Type", reinterpret_cast<int*>( &mBuildData.meshType ), MeshTypeStrings );
    edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.meshType ), MeshType_Count );
    
    if( ImGui::ColorEdit3( "Mesh Colour", mBuildData.color.data() ) )
    {        
        mShader.SetColorTint( mBuildData.color );
        ImGuiExtra::MarkSettingsDirty();
    }

    edited |= ImGui::DragInt( "Seed", &mBuildData.seed );
    edited |= ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.0005f, 0, 0, "%.4f" );

    if( mBuildData.meshType == MeshType_Heightmap2D )
    {
        edited |= ImGui::DragFloat( "Heightmap Multiplier", &mBuildData.heightmapMultiplier, 0.5f );        
    }
    else
    {
        edited |= ImGui::DragFloat( "Iso Surface", &mBuildData.isoSurface, 0.02f );
    }

    if( edited )
    {
        ReGenerate( mBuildData.generator );
        ImGuiExtra::MarkSettingsDirty();
    }

    float triLimitMil = (float)mTriLimit / 1000000.0f;
    if( ImGui::DragFloat( "Triangle Limit", &triLimitMil, 1, 10.0f, 300.0f, "%0.1fM" ) )
    {
        mTriLimit = (uint32_t)( triLimitMil * 1000000 );
        ImGuiExtra::MarkSettingsDirty();
    }

    ImGui::Text( "Triangle Count: %0.1fM (%0.1fM)", mTriCount / 1000000.0f, drawnTriCount / 3000000.0f );
    ImGui::Text( "Voxel Count: %0.1fM", ( mChunks.size() * Chunk::SIZE * Chunk::SIZE * Chunk::SIZE ) / 1000000.0 );
    ImGui::Text( "Loaded Chunks: %zu (%d)", mChunks.size(), mMeshesCount );

    size_t generateCount = mGenerateQueue.Count();
    ImGui::Text( "Meshing Chunks: %zu (%zu)", mRegisteredChunkPositions.size() - mChunks.size() - generateCount, generateCount );
    ImGui::Text( "Chunk Load Range: %0.1f", mLoadRange );
    ImGui::Text( "Generated Min (%0.6f) : Max (%0.6f)", mMinMax.min, mMinMax.max );

    if( mBuildData.meshType != MeshType_Heightmap2D )
    {
        ImGui::Text( "Min Air Y (%0.1f) : Max Solid Y (%0.1f)", mMinAirY, mMaxSolidY );
    }

    ImGui::Text( "Camera Pos: %0.1f, %0.1f, %0.1f", cameraPosition.x(), cameraPosition.y(), cameraPosition.z() );

    UpdateChunksForPosition( cameraPosition );
}

float MeshNoisePreview::GetLoadRangeModifier()
{
    return std::min( 0.01f, (float)(1000 / std::pow( std::min( 1000.0f, mLoadRange ), 1.5 ) ) );
}

void MeshNoisePreview::UpdateChunkQueues( const Vector3& position )
{
    size_t queueCount = mCompleteQueue.Count();

    if( mTriCount > mTriLimit ) // Reduce load range if over tri limit
    {
        mLoadRange = std::max( mLoadRange * (1 - GetLoadRangeModifier()), Chunk::SIZE * 1.5f );
    }

    StartTimer();
    Vector3i chunkPos = Vector3i( position - Vector3( Chunk::SIZE / 2.0f ) );

    size_t newChunks = 0;
    if( queueCount )
    {        
        Chunk::MeshData meshData;

        while( GetTimerDurationMs() < 14 && mCompleteQueue.Pop( meshData ) )
        {
            mMinMax << meshData.minMax;
            mMinAirY = std::min( mMinAirY, meshData.minAirY );
            mMaxSolidY = std::max( mMaxSolidY, meshData.maxSolidY );
            
            mChunks.emplace_back( meshData );
            newChunks++;
        }
        mAvgNewChunks += (newChunks - mAvgNewChunks) * 0.01f;
    }

    std::sort( mChunks.begin(), mChunks.end(), 
        [chunkPos]( const Chunk& a, const Chunk& b )
        {
            return (chunkPos - a.GetPos()).dot() < (chunkPos - b.GetPos()).dot();
        } );

    // Unload further chunk if out of load range
    size_t deletedChunks = 0;
    while( !mChunks.empty() )
    {
        Vector3i backChunkPos = mChunks.back().GetPos();
        float unloadRange = mLoadRange * 1.1f;
        if( GetTimerDurationMs() < 15 && (chunkPos - backChunkPos).dot() > unloadRange * unloadRange )
        {
            mRegisteredChunkPositions.erase( backChunkPos );
            mChunks.pop_back();
            deletedChunks++;
        }
        else
        {
            break;
        }
    }

    //ImGui::Text( " Queued Chunks: %zu", queueCount );
    //ImGui::Text( "    New Chunks: %zu (%0.1f)", newChunks, mAvgNewChunks );
    //ImGui::Text( "Deleted Chunks: %zu", deletedChunks );

    // Increase load range if queue is not full
    if( (double)mTriCount < mTriLimit * 0.85 && (mRegisteredChunkPositions.size() - mChunks.size()) < mThreads.size() * mAvgNewChunks )
    {
        mLoadRange = std::min( mLoadRange * (1 + GetLoadRangeModifier()), 3000.0f );
    }

}

void MeshNoisePreview::UpdateChunksForPosition( Vector3 position )
{
    //StartTimer();
    int chunkRange = (int)ceilf( mLoadRange / Chunk::SIZE );

    position -= Vector3( Chunk::SIZE * 0.5f );
    Vector3i positionI = Vector3i( position );

    Vector3i chunkCenter = (positionI / Chunk::SIZE) * Chunk::SIZE;

    std::vector<Vector3i> chunkPositions;
    Vector3i chunkPos;
    int loadRangeSq = (int)(mLoadRange * mLoadRange);

    int staggerShift = std::min( 5, (int)((loadRangeSq * (int64_t)mLoadRange) / 1000000000) );
    int staggerCount = (1 << staggerShift) - 1;

    for( int x = -chunkRange; x <= chunkRange; x++ )
    {
        if( (x & staggerCount) != (mStaggerCheck & staggerCount) )
        {
            continue;
        }

        chunkPos.x() = x * Chunk::SIZE + chunkCenter.x();

        for( int y = -chunkRange; y <= chunkRange; y++ )
        {
            if( mBuildData.meshType == MeshType_Heightmap2D )
            {
                positionI.y() = 0;
                chunkPos.y() = 0;
                y = chunkRange;
            }
            else
            {
                chunkPos.y() = y * Chunk::SIZE + chunkCenter.y();
            }

            for( int z = -chunkRange; z <= chunkRange; z++ )
            {
                chunkPos.z() = z * Chunk::SIZE + chunkCenter.z();


                if( ( positionI - chunkPos ).dot() <= loadRangeSq &&
                    !mRegisteredChunkPositions.contains( chunkPos ) )
                {
                    chunkPositions.push_back( chunkPos );
                }
            }
        }
    }

    mStaggerCheck++;

    std::sort( chunkPositions.begin(), chunkPositions.end(), [positionI]( const Vector3i& a, const Vector3i& b )
    {
        return (positionI - a).dot() < (positionI - b).dot();
    } );

    for( const Vector3i& pos : chunkPositions )
    {
        mBuildData.pos = pos;
        mRegisteredChunkPositions.insert( pos );

        if( mGenerateQueue.Push( mBuildData ) >= mThreads.size() * 16 )
        {
            break;
        }
    }

    //ImGui::Text( "UpdateChunksForPosition(%d) Ms: %.2f", staggerShift, GetTimerDurationMs() );
}

void MeshNoisePreview::GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue )
{
    while( true )
    {
        Chunk::BuildData buildData = generateQueue.Pop();

        if( generateQueue.ShouldKillThread() )
        {
            return;
        }

        Chunk::MeshData meshData = Chunk::BuildMeshData( buildData );

        if( !completeQueue.Push( meshData, buildData.genVersion ) )
        {
            meshData.Free();
        }
    }
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildMeshData( const BuildData& buildData )
{
    thread_local static std::vector<float> densityValues( SIZE_GEN * SIZE_GEN * SIZE_GEN );
    thread_local static std::vector<VertexData> vertexData;
    thread_local static std::vector<uint32_t> indicies;
    
    vertexData.clear();
    indicies.clear();
    
    switch( buildData.meshType )
    {
    case MeshType_Voxel3D:
        return BuildVoxel3DMesh( buildData, densityValues.data(), vertexData, indicies );

    case MeshType_Heightmap2D:
        return BuildHeightMap2DMesh( buildData, densityValues.data(), vertexData, indicies );

    case MeshType_Count:
        break;
    }           

    return MeshData( buildData.pos, {}, vertexData, indicies );
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildVoxel3DMesh( const BuildData& buildData, float* densityValues, std::vector<VertexData>& vertexData, std::vector<uint32_t>& indicies )
{
    FastNoise::OutputMinMax minMax = buildData.generator->GenUniformGrid3D( densityValues,
                                                                            buildData.pos.x() - 1, buildData.pos.y() - 1, buildData.pos.z() - 1,
                                                                            SIZE_GEN, SIZE_GEN, SIZE_GEN, buildData.frequency, buildData.seed );
    float minAir = INFINITY;
    float maxSolid = -INFINITY;

#if FASTNOISE_CALC_MIN_MAX
    if( minMax.min > buildData.isoSurface )
    {
        minAir = (float)buildData.pos.y();
    }
    else if( minMax.max < buildData.isoSurface )
    {
        maxSolid = (float)buildData.pos.y() - 1.0f + SIZE;
    }
    else
#endif
    {
        Vector3 light = LIGHT_DIR.normalized() * ( 1.0f - AMBIENT_LIGHT ) + Vector3( AMBIENT_LIGHT );

        float xLight = std::abs( light.x() );
        float yLight = std::abs( light.y() );
        float zLight = std::abs( light.z() );

        constexpr int32_t STEP_X = 1;
        constexpr int32_t STEP_Y = SIZE_GEN;
        constexpr int32_t STEP_Z = SIZE_GEN * SIZE_GEN;

        int32_t noiseIdx = STEP_X + STEP_Y + STEP_Z;

        for( uint32_t z = 0; z < SIZE; z++ )
        {
            float zf = z + (float)buildData.pos.z();

            for( uint32_t y = 0; y < SIZE; y++ )
            {
                float yf = y + (float)buildData.pos.y();

                for( uint32_t x = 0; x < SIZE; x++ )
                {
                    float xf = x + (float)buildData.pos.x();

                    if( densityValues[noiseIdx] <= buildData.isoSurface ) // Is Solid?
                    {
                        maxSolid = std::max( yf, maxSolid );

                        if( densityValues[noiseIdx + STEP_X] > buildData.isoSurface ) // Right
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_X, STEP_Y, STEP_Z, xLight,
                                       Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_X] > buildData.isoSurface ) // Left
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_X, -STEP_Y, STEP_Z, 1.0f - xLight,
                                       Vector3( xf, yf + 1, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Y] > buildData.isoSurface ) // Up
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_Y, STEP_Z, STEP_X, yLight,
                                       Vector3( xf, yf + 1, zf ), Vector3( xf, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf ) );
                        }

                        if( densityValues[noiseIdx - STEP_Y] > buildData.isoSurface ) // Down
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_Y, -STEP_Z, STEP_X, 1.0f - yLight,
                                       Vector3( xf, yf, zf + 1 ), Vector3( xf, yf, zf ), Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Z] > buildData.isoSurface ) // Forward
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_Z, STEP_X, STEP_Y, zLight,
                                       Vector3( xf, yf, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_Z] > buildData.isoSurface ) // Back
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_Z, -STEP_X, STEP_Y, 1.0f - zLight,
                                       Vector3( xf + 1, yf, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf ) );
                        }
                    }
                    else
                    {
                        minAir = std::min( yf, minAir );
                    }
                    noiseIdx++;
                }

                noiseIdx += STEP_X * 2;
            }

            noiseIdx += STEP_Y * 2;
        }
    }

    return MeshData( buildData.pos, minMax, vertexData, indicies, minAir, maxSolid );
}

void MeshNoisePreview::Chunk::AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
                                         int32_t idx, int32_t facingOffset, int32_t offsetA, int32_t offsetB, float light, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 )
{
    int32_t facingIdx = idx + facingOffset;

    uint8_t sideA0 = density[facingIdx - offsetA] <= isoSurface;
    uint8_t sideA1 = density[facingIdx + offsetA] <= isoSurface;
    uint8_t sideB0 = density[facingIdx - offsetB] <= isoSurface;
    uint8_t sideB1 = density[facingIdx + offsetB] <= isoSurface;
    
    uint8_t corner00 = (sideA0 & sideB0) || density[facingIdx - offsetA - offsetB] <= isoSurface;
    uint8_t corner01 = (sideA0 & sideB1) || density[facingIdx - offsetA + offsetB] <= isoSurface;
    uint8_t corner10 = (sideA1 & sideB0) || density[facingIdx + offsetA - offsetB] <= isoSurface;
    uint8_t corner11 = (sideA1 & sideB1) || density[facingIdx + offsetA + offsetB] <= isoSurface;

    constexpr float aoAdjust = AO_STRENGTH / 3.0f; 

    float ao00 = (float)(sideA0 + sideB0 + corner00) * aoAdjust;
    float ao01 = (float)(sideA1 + sideB0 + corner10) * aoAdjust;
    float ao10 = (float)(sideA0 + sideB1 + corner01) * aoAdjust;
    float ao11 = (float)(sideA1 + sideB1 + corner11) * aoAdjust;

    float densityLightShift = 1 - (isoSurface - density[idx]) * 2;
    light *= densityLightShift * densityLightShift;

    uint32_t vertIdx = (uint32_t)verts.size();
    verts.emplace_back( pos00, (1.0f - ao00) * light );
    verts.emplace_back( pos01, (1.0f - ao01) * light );
    verts.emplace_back( pos10, (1.0f - ao10) * light );
    verts.emplace_back( pos11, (1.0f - ao11) * light );

    // Rotate tris to give best visuals for AO lighting
    uint32_t triRotation = ( ao00 + ao11 > ao01 + ao10 ) * 2;    
    indicies.push_back( vertIdx );
    indicies.push_back( vertIdx + 3 - triRotation );
    indicies.push_back( vertIdx + 2 );
    indicies.push_back( vertIdx + 3 );
    indicies.push_back( vertIdx + triRotation );
    indicies.push_back( vertIdx + 1 );
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildHeightMap2DMesh( const BuildData& buildData, float* densityValues, std::vector<VertexData>& vertexData, std::vector<uint32_t>& indicies )
{
    constexpr uint32_t SIZE_GEN_HEIGHTMAP = SIZE + 1;

    FastNoise::OutputMinMax minMax = buildData.generator->GenUniformGrid2D( densityValues,
                                                                            buildData.pos.x(), buildData.pos.z(),
                                                                            SIZE_GEN_HEIGHTMAP, SIZE_GEN_HEIGHTMAP, buildData.frequency, buildData.seed );
    constexpr int32_t STEP_X = 1;
    constexpr int32_t STEP_Y = SIZE_GEN_HEIGHTMAP;

    Vector3 sunLight = LIGHT_DIR.normalized() * ( 1.0f - AMBIENT_LIGHT ) + Vector3( AMBIENT_LIGHT );

    int32_t noiseIdx = 0;

    for( uint32_t y = 0; y < SIZE; y++ )
    {
        float yf = y + (float)buildData.pos.z();

        for( uint32_t x = 0; x < SIZE; x++ )
        {
            float xf = x + (float)buildData.pos.x();

            Vector3 v00( xf, densityValues[noiseIdx] * buildData.heightmapMultiplier, yf );
            Vector3 v01( xf, densityValues[noiseIdx + STEP_Y] * buildData.heightmapMultiplier, yf + 1 );
            Vector3 v10( xf + 1, densityValues[noiseIdx + STEP_X] * buildData.heightmapMultiplier, yf );
            Vector3 v11( xf + 1, densityValues[noiseIdx + STEP_X + STEP_Y] * buildData.heightmapMultiplier, yf + 1 );            

            // Normal for quad
            float light = ( sunLight * (
                Math::cross( v10 - v11, v00 - v11 ).normalized() +
                Math::cross( v01 - v00, v11 - v00 ).normalized() ).normalized() ).dot();

            uint32_t vertIdx = (uint32_t)vertexData.size();
            vertexData.emplace_back( v00, light );
            vertexData.emplace_back( v01, light );
            vertexData.emplace_back( v10, light );
            vertexData.emplace_back( v11, light );

            // Slice quad along longest split
            uint32_t triRotation = 2 * ( (v00 + v11).dot() < (v01 + v10).dot() );            
            indicies.push_back( vertIdx );
            indicies.push_back( vertIdx + 3 - triRotation );
            indicies.push_back( vertIdx + 2 );
            indicies.push_back( vertIdx + 3 );
            indicies.push_back( vertIdx + triRotation );
            indicies.push_back( vertIdx + 1 );

            noiseIdx++;
        }

        noiseIdx += STEP_X;
    }

    return MeshData( buildData.pos, minMax, vertexData, indicies );
}

MeshNoisePreview::Chunk::Chunk( MeshData& meshData )
{
    mPos = meshData.pos;

    if( !meshData.vertexData.isEmpty() )
    {
        //https://doc.magnum.graphics/magnum/classMagnum_1_1GL_1_1Mesh.html

        mMesh = std::make_unique<GL::Mesh>( GL::MeshPrimitive::Triangles );

        mMesh->addVertexBuffer( GL::Buffer( GL::Buffer::TargetHint::Array, meshData.vertexData ), 0, VertexLightShader::PositionLight{} );

        if( meshData.indicies.isEmpty() )
        {
            mMesh->setCount( (int)meshData.vertexData.size() );
        }
        else
        {
            mMesh->setCount( (Int)meshData.indicies.size() );
            mMesh->setIndexBuffer( GL::Buffer( GL::Buffer::TargetHint::ElementArray, meshData.indicies ), 0, GL::MeshIndexType::UnsignedInt, 0, (UnsignedInt)meshData.vertexData.size() - 1 );
        }
    }

    meshData.Free();
}

MeshNoisePreview::VertexLightShader::VertexLightShader()
{
    Utility::Resource noiseToolResources( "NoiseTool" );

#ifndef MAGNUM_TARGET_GLES
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GL320, GL::Version::GL310, GL::Version::GL300, GL::Version::GL210 } );
#else
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GLES300, GL::Version::GLES200 } );
#endif
    
    GL::Shader vert = CreateShader( version, GL::Shader::Type::Vertex );
    GL::Shader frag = CreateShader( version, GL::Shader::Type::Fragment );
    
    CORRADE_INTERNAL_ASSERT_OUTPUT(
        vert.addSource( noiseToolResources.getString( "VertexLight.vert" ) ).compile() );
    CORRADE_INTERNAL_ASSERT_OUTPUT( 
        frag.addSource( noiseToolResources.getString( "VertexLight.frag" ) ).compile() );

    attachShader( vert );
    attachShader( frag );

    /* ES3 has this done in the shader directly */
#if !defined(MAGNUM_TARGET_GLES) || defined(MAGNUM_TARGET_GLES2)
#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_attrib_location>( version ) )
#endif
    {
        bindAttributeLocation( PositionLight::Location, "positionLight" );
    }
#endif

    CORRADE_INTERNAL_ASSERT_OUTPUT( link() );

#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_uniform_location>( version ) )
#endif
    {
        mTransformationProjectionMatrixUniform = uniformLocation( "transformationProjectionMatrix" );
        mColorTintUniform = uniformLocation( "colorTint" );
    }

    /* Set defaults in OpenGL ES (for desktop they are set in shader code itself) */
#ifdef MAGNUM_TARGET_GLES
    SetTransformationProjectionMatrix( Matrix4{} );
    SetColorTint( Color3 { 1.0f } );
#endif
}

GL::Shader MeshNoisePreview::VertexLightShader::CreateShader( GL::Version version, GL::Shader::Type type )
{
    GL::Shader shader( version, type );

#ifndef MAGNUM_TARGET_GLES
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::explicit_attrib_location>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_explicit_attrib_location\n" );
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::shading_language_420pack>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_shading_language_420pack\n" );
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::explicit_uniform_location>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_explicit_uniform_location\n" );
#endif

#ifndef MAGNUM_TARGET_GLES2
    if( type == GL::Shader::Type::Vertex && GL::Context::current().isExtensionDisabled<GL::Extensions::MAGNUM::shader_vertex_id>( version ) )
        shader.addSource( "#define DISABLE_GL_MAGNUM_shader_vertex_id\n" );
#endif

/* My Android emulator (running on NVidia) doesn't define GL_ES
       preprocessor macro, thus *all* the stock shaders fail to compile */
/** @todo remove this when Android emulator is sane */
#ifdef CORRADE_TARGET_ANDROID
    shader.addSource( "#ifndef GL_ES\n#define GL_ES 1\n#endif\n" );
#endif

    return shader;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetTransformationProjectionMatrix( const Matrix4& matrix )
{
    setUniform( mTransformationProjectionMatrixUniform, matrix );
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetColorTint( const Color3& color )
{
    setUniform( mColorTintUniform, Vector4( color, 1.0f ) );
    return *this;
}

void MeshNoisePreview::StartTimer()
{
    mTimerStart = std::chrono::high_resolution_clock::now();
}

float MeshNoisePreview::GetTimerDurationMs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - mTimerStart).count() / 1e3f;
}

void MeshNoisePreview::SetupSettingsHandlers()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName = "NoiseToolMeshNoisePreview";
    editorSettings.TypeHash = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* meshNoisePreview = (MeshNoisePreview*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        outBuf->appendf( "tri_limit=%d\n", (int)meshNoisePreview->mTriLimit );
        outBuf->appendf( "frequency=%f\n", meshNoisePreview->mBuildData.frequency );
        outBuf->appendf( "iso_surface=%f\n", meshNoisePreview->mBuildData.isoSurface );
        outBuf->appendf( "heightmap_multiplier=%f\n", meshNoisePreview->mBuildData.heightmapMultiplier );
        outBuf->appendf( "seed=%d\n", meshNoisePreview->mBuildData.seed );
        outBuf->appendf( "color=%d\n", (int)meshNoisePreview->mBuildData.color.toSrgbInt() );
        outBuf->appendf( "mesh_type=%d\n", (int)meshNoisePreview->mBuildData.meshType );
        outBuf->appendf( "enabled=%d\n", (int)meshNoisePreview->mEnabled );
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* meshNoisePreview = (MeshNoisePreview*)handler->UserData;

        sscanf( line, "tri_limit=%d", &meshNoisePreview->mTriLimit );
        sscanf( line, "frequency=%f", &meshNoisePreview->mBuildData.frequency );
        sscanf( line, "iso_surface=%f", &meshNoisePreview->mBuildData.isoSurface );
        sscanf( line, "heightmap_multiplier=%f", &meshNoisePreview->mBuildData.heightmapMultiplier );
        sscanf( line, "seed=%d", &meshNoisePreview->mBuildData.seed );
        sscanf( line, "mesh_type=%d", (int*)&meshNoisePreview->mBuildData.meshType );

        int i;
        if( sscanf( line, "color=%d", &i ) == 1 )
        {
            meshNoisePreview->mBuildData.color = Color3::fromSrgbInt( i );
        }
        else if( sscanf( line, "enabled=%d", &i ) == 1 )
        {
            meshNoisePreview->mEnabled = i;
        }
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}