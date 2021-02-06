#include "MeshNoisePreview.h"

#include <algorithm>
#include <imgui.h>
#include <thread>
#include <cmath>

#include <Corrade/Utility/Resource.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Frustum.h>
#include <Magnum/Math/Intersection.h>
#include <Magnum/Shaders/Implementation/CreateCompatibilityShader.h>

using namespace Magnum;

MeshNoisePreview::MeshNoisePreview()
{
    mBuildData.frequency = 0.005f;
    mBuildData.seed = 1338;
    mBuildData.isoSurface = 0.0f;
    mBuildData.color = Color3( 1.0f );

    uint32_t threadCount = std::max( 2u, std::thread::hardware_concurrency() );

    threadCount -= threadCount / 4;

    for( uint32_t i = 0; i < threadCount; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }
}

MeshNoisePreview::~MeshNoisePreview()
{
    for( auto& thread : mThreads )
    {
        mGenerateQueue.KillThreads();
        thread.join();
    }
}

void MeshNoisePreview::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mLoadRange = 200.0f;
    mBuildData.generator = generator;
    mBuildData.pos = Vector3i( 0 );
    mMinMax = {};

    mChunks.clear();
    mInProgressChunks.clear();
    mDistanceOrderedChunks.clear();
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
    }

    if( !mBuildData.generator || !mEnabled )
    {
        return;
    }

    UpdateChunkQueues( cameraPosition );

    Matrix4 transformationProjection = projection * transformation;

    Frustum camFrustrum = Frustum::fromMatrix( transformationProjection );
    mShader.setTransformationProjectionMatrix( transformationProjection );

    mTriCount = 0;
    uint32_t drawnTriCount = 0;

    for( const Vector3i& pos : mDistanceOrderedChunks )
    {
        GL::Mesh& mesh = mChunks.at( pos ).GetMesh();

        if( uint32_t meshTriCount = mesh.count() )
        {
            mTriCount += meshTriCount;

            Range3Di bbox( pos, pos + Vector3i( Chunk::SIZE + 1 ) );

            if( Math::Intersection::rangeFrustum( Range3D( bbox ), camFrustrum ) )
            {
                drawnTriCount += meshTriCount;
                mShader.draw( mesh );
            }
        }
    }
    mTriCount /= 3;

    //ImGui::Text( "Generating Queue Count: %llu", mGenerateQueue.Count() );
    ImGui::Text( "Triangle Count: %0.1fM (%0.1fM)", mTriCount / 1000000.0f, drawnTriCount / 3000000.0f );
    ImGui::Text( "   Voxel Count: %0.1fK", (mChunks.size() * Chunk::SIZE * Chunk::SIZE * Chunk::SIZE) / 1000.0 );
    ImGui::Text( "Chunk Load Range: %0.1f", mLoadRange );
    ImGui::Text( "Generated Min(%0.6f) Max(%0.6f)", mMinMax.min, mMinMax.max );

    float triLimitMil = (float)mTriLimit / 1000000.0f;
    if( ImGui::DragFloat( "Triangle Limit", &triLimitMil, 1, 10.0f, 300.0f, "%0.1fM" ) )
    {
        mTriLimit = (uint32_t)(triLimitMil * 1000000);
    }


    if( ImGui::ColorEdit3( "Mesh Colour", mBuildData.color.data() ) |
        ImGui::DragInt( "Seed", &mBuildData.seed ) |
        ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.0005f, 0, 0, "%.4f" ) |
        ImGui::DragFloat( "Iso Surface", &mBuildData.isoSurface, 0.02f ) )
    {
        ReGenerate( mBuildData.generator );
    }

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
            mInProgressChunks.erase( meshData.pos );
            mDistanceOrderedChunks.push_back( meshData.pos );

            mMinMax << meshData.minMax;
            mChunks.emplace( meshData.pos, meshData );
            newChunks++;
        }
        mAvgNewChunks += (newChunks - mAvgNewChunks) * 0.01f;
    }

    std::sort( mDistanceOrderedChunks.begin(), mDistanceOrderedChunks.end(), 
        [chunkPos]( const Vector3i& a, const Vector3i& b )
        {
            return (chunkPos - a).dot() < (chunkPos - b).dot();
        } );

    // Unload further chunk if out of load range
    size_t deletedChunks = 0;
    while( !mDistanceOrderedChunks.empty() )
    {
        Vector3i backChunkPos = mDistanceOrderedChunks.back();
        float unloadRange = mLoadRange * 1.1f;
        if( GetTimerDurationMs() < 15 && (chunkPos - backChunkPos).dot() > unloadRange * unloadRange )
        {
            mChunks.erase( backChunkPos );
            mDistanceOrderedChunks.pop_back();
            deletedChunks++;
        }
        else
        {
            break;
        }
    }

    ImGui::Text( "Meshing Chunks: %zu", mInProgressChunks.size() - mCompleteQueue.Count() );
    ImGui::Text( " Queued Chunks: %zu", queueCount );
    ImGui::Text( "    New Chunks: %zu (%0.1f)", newChunks, mAvgNewChunks );
    ImGui::Text( "Deleted Chunks: %zu", deletedChunks );
    ImGui::Text( " Loaded Chunks: %zu", mDistanceOrderedChunks.size() );

    // Increase load range if queue is not full
    if( (double)mTriCount < mTriLimit * 0.85 && mInProgressChunks.size() < 20 * mAvgNewChunks )
    {
        mLoadRange = std::min( mLoadRange * (1 + GetLoadRangeModifier()), 2000.0f );
    }

}

void MeshNoisePreview::UpdateChunksForPosition( Vector3 position )
{
    //StartTimer();
    int chunkRange = (int)ceilf( mLoadRange / Chunk::SIZE );

    position -= Vector3( Chunk::SIZE / 2.0f );
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
            chunkPos.y() = y * Chunk::SIZE + chunkCenter.y();

            for( int z = -chunkRange; z <= chunkRange; z++ )
            {
                chunkPos.z() = z * Chunk::SIZE + chunkCenter.z();

                if( (positionI - chunkPos).dot() <= loadRangeSq && 
                    mChunks.find( chunkPos ) == mChunks.end() && 
                    mInProgressChunks.find( chunkPos ) == mInProgressChunks.end() )
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
        mInProgressChunks.insert( pos );

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

    FastNoise::OutputMinMax minMax = buildData.generator->GenUniformGrid3D( densityValues.data(),
        buildData.pos.x() - 1, buildData.pos.y() - 1, buildData.pos.z() - 1,
        SIZE_GEN, SIZE_GEN, SIZE_GEN, buildData.frequency, buildData.seed );

    vertexData.clear();
    indicies.clear();

#if FASTNOISE_CALC_MIN_MAX
    if( minMax.min <= buildData.isoSurface && minMax.max >= buildData.isoSurface )
#endif
    {
        Vector3 light = LIGHT_DIR.normalized() * (1.0f - AMBIENT_LIGHT) + Vector3( AMBIENT_LIGHT );

        float xLight = std::abs( light.x() );
        Color3 colorRight = buildData.color * xLight;
        Color3 colorLeft = buildData.color * (1.0f - xLight);

        float yLight = std::abs( light.y() );
        Color3 colorUp = buildData.color * yLight;
        Color3 colorDown = buildData.color * (1.0f - yLight);

        float zLight = std::abs( light.z() );
        Color3 colorForward = buildData.color * zLight;
        Color3 colorBack = buildData.color * (1.0f - zLight);

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

                    if( densityValues[noiseIdx] <= buildData.isoSurface ) // isSolid
                    {
                        if( densityValues[noiseIdx + STEP_X] > buildData.isoSurface ) // Right
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, STEP_X, STEP_Y, STEP_Z, colorRight,
                                Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_X] > buildData.isoSurface ) // Left
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, -STEP_X, -STEP_Y, STEP_Z, colorLeft,
                                Vector3( xf, yf + 1, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Y] > buildData.isoSurface ) // Up
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, STEP_Y, STEP_Z, STEP_X, colorUp,
                                Vector3( xf, yf + 1, zf ), Vector3( xf, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf ) );
                        }

                        if( densityValues[noiseIdx - STEP_Y] > buildData.isoSurface ) // Down
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, -STEP_Y, -STEP_Z, STEP_X, colorDown,
                                Vector3( xf, yf, zf + 1 ), Vector3( xf, yf, zf ), Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Z] > buildData.isoSurface ) // Forward
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, STEP_Z, STEP_X, STEP_Y, colorForward,
                                Vector3( xf, yf, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_Z] > buildData.isoSurface ) // Back
                        {
                            AddQuadAO( vertexData, indicies, densityValues.data(), buildData.isoSurface, noiseIdx, -STEP_Z, -STEP_X, STEP_Y, colorBack,
                                Vector3( xf + 1, yf, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf ) );
                        }
                    }
                    noiseIdx++;
                }

                noiseIdx += STEP_X * 2;
            }

            noiseIdx += STEP_Y * 2;
        }
    }

    MeshData meshData( buildData.pos, minMax, vertexData, indicies );

    return meshData;
}

MeshNoisePreview::Chunk::Chunk( MeshData& meshData )
{
    mPos = meshData.pos;

    if( !meshData.vertexData.empty() )
    {
        //https://doc.magnum.graphics/magnum/classMagnum_1_1GL_1_1Mesh.html

        mMesh = GL::Mesh( GL::MeshPrimitive::Triangles );

        mMesh.setCount( (Int)meshData.indicies.size() )
            .setIndexBuffer( GL::Buffer( GL::Buffer::TargetHint::ElementArray, meshData.indicies ), 0, GL::MeshIndexType::UnsignedInt, 0, (UnsignedInt)meshData.vertexData.size() - 1 )
            .addVertexBuffer( GL::Buffer( GL::Buffer::TargetHint::Array, meshData.vertexData ), 0, VertexColorShader::Position{}, VertexColorShader::Color3{} );
    }

    meshData.Free();
}

void MeshNoisePreview::Chunk::AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
    int32_t idx, int32_t facingOffset, int32_t offsetA, int32_t offsetB, Color3 color, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 )
{
    int32_t facingIdx = idx + facingOffset;

    uint8_t sideA0 = density[facingIdx - offsetA] <= isoSurface;
    uint8_t sideA1 = density[facingIdx + offsetA] <= isoSurface;
    uint8_t sideB0 = density[facingIdx - offsetB] <= isoSurface;
    uint8_t sideB1 = density[facingIdx + offsetB] <= isoSurface;

    uint8_t corner00 = (sideA0 && sideB0) || density[facingIdx - offsetA - offsetB] <= isoSurface;
    uint8_t corner01 = (sideA0 && sideB1) || density[facingIdx - offsetA + offsetB] <= isoSurface;
    uint8_t corner10 = (sideA1 && sideB0) || density[facingIdx + offsetA - offsetB] <= isoSurface;
    uint8_t corner11 = (sideA1 && sideB1) || density[facingIdx + offsetA + offsetB] <= isoSurface;

    float light00 = (float)(sideA0 + sideB0 + corner00) / 3.0f;
    float light01 = (float)(sideA1 + sideB0 + corner10) / 3.0f;
    float light10 = (float)(sideA0 + sideB1 + corner01) / 3.0f;
    float light11 = (float)(sideA1 + sideB1 + corner11) / 3.0f;

    float densityColorShift = 1 - (isoSurface - density[idx]) * 2;

    color *= densityColorShift * densityColorShift;

    uint32_t vertIdx = (uint32_t)verts.size();
    verts.emplace_back( pos00, color * (1.0f - light00 * AO_STRENGTH) );
    verts.emplace_back( pos01, color * (1.0f - light01 * AO_STRENGTH) );
    verts.emplace_back( pos10, color * (1.0f - light10 * AO_STRENGTH) );
    verts.emplace_back( pos11, color * (1.0f - light11 * AO_STRENGTH) );

    if( light00 + light11 < light01 + light10 )
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

MeshNoisePreview::VertexColorShader::VertexColorShader()
{
#ifdef MAGNUM_BUILD_STATIC
    if( !Utility::Resource::hasGroup( "MagnumShaders" ) )
        importShaderResources();
#endif

    Utility::Resource rs( "MagnumShaders" );

#ifndef MAGNUM_TARGET_GLES
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GL320, GL::Version::GL310, GL::Version::GL300, GL::Version::GL210 } );
#else
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GLES300, GL::Version::GLES200 } );
#endif

    std::string fragShader( rs.get( "VertexColor.frag" ) );
    std::string mainStart( "void main() {" );
    size_t mainStartIdx = fragShader.find( mainStart );

    CORRADE_INTERNAL_ASSERT_OUTPUT( mainStartIdx != std::string::npos );
    fragShader.insert( mainStartIdx + mainStart.length(), "if( !gl_FrontFacing ){ fragmentColor = vec4(0.0,0.0,0.0,1.0); return; }" );

    GL::Shader vert = Shaders::Implementation::createCompatibilityShader( rs, version, GL::Shader::Type::Vertex );
    GL::Shader frag = Shaders::Implementation::createCompatibilityShader( rs, version, GL::Shader::Type::Fragment );

    CORRADE_INTERNAL_ASSERT_OUTPUT(
        vert.addSource( "#define THREE_DIMENSIONS\n" )
            .addSource( rs.get( "generic.glsl" ) )
            .addSource( rs.get( "VertexColor.vert" ) ).compile() );
    CORRADE_INTERNAL_ASSERT_OUTPUT( 
        frag.addSource( rs.get( "generic.glsl" ) )
            .addSource( fragShader ).compile() );

    attachShader( vert );
    attachShader( frag );

    /* ES3 has this done in the shader directly */
#if !defined(MAGNUM_TARGET_GLES) || defined(MAGNUM_TARGET_GLES2)
#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_attrib_location>( version ) )
#endif
    {
        bindAttributeLocation( Position::Location, "position" );
        bindAttributeLocation( Color3::Location, "color" ); /* Color4 is the same */
    }
#endif

    CORRADE_INTERNAL_ASSERT_OUTPUT( link() );

#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_uniform_location>( version ) )
#endif
    {
        mTransformationProjectionMatrixUniform = uniformLocation( "transformationProjectionMatrix" );
    }

    /* Set defaults in OpenGL ES (for desktop they are set in shader code itself) */
#ifdef MAGNUM_TARGET_GLES
    setTransformationProjectionMatrix( Matrix4{} );
#endif
}

MeshNoisePreview::VertexColorShader& MeshNoisePreview::VertexColorShader::setTransformationProjectionMatrix( const Matrix4& matrix )
{
    setUniform( mTransformationProjectionMatrixUniform, matrix );
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
