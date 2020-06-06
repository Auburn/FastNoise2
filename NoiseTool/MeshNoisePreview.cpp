#include "MeshNoisePreview.h"

#include <imgui.h>

#include <Corrade/Utility/Resource.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/Implementation/CreateCompatibilityShader.h>

using namespace Magnum;

MeshNoisePreview::MeshNoisePreview()
{
    mBuildData.frequency = 0.02f;
    mBuildData.isoSurface = 0.0f;
    mBuildData.color = Color3( 1.0f );

    uint32_t threadCount = std::max( 2u, std::thread::hardware_concurrency() );

    threadCount -= threadCount / 4;

    for( uint32_t i = 0; i < threadCount; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }
}

void MeshNoisePreview::ReGenerate( const std::shared_ptr<FastNoise::Generator>& generator, int32_t seed )
{
    mBuildData.generator = generator;
    mBuildData.seed = seed;
    mBuildData.pos = Vector3i( 0 );
    mBuildData.genVersion = mCompleteQueue.IncVersion();

    mChunks.clear();
    mInProgressChunks.clear();
    mGenerateQueue.Clear();

    Chunk::MeshData meshData;
    while( mCompleteQueue.Pop( meshData ) )
    {
        meshData.Free();
    }
}

void MeshNoisePreview::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition )
{
    Chunk::MeshData meshData;
    if( mCompleteQueue.Pop( meshData ) )
    {
        mInProgressChunks.erase( meshData.pos );
        mChunks.emplace_back( meshData );
    }

    mShader.setTransformationProjectionMatrix( projection * transformation );

    size_t triCount = 0;

    for( Chunk& chunk : mChunks )
    {
        if( chunk.GetMesh().count() )
        {
            triCount += chunk.GetMesh().count() / 3;
            chunk.GetMesh().draw( mShader );
        }
    }

    ImGui::Text( "Thread Count: %llu", mThreads.size() );
    ImGui::Text( "Triangle Count: %0.1fK", triCount / 1000.0 );
    ImGui::Text( "Voxel Count: %0.1fK", (mChunks.size() * Chunk::SIZE * Chunk::SIZE * Chunk::SIZE) / 1000.0 );

    if( ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.001f ) )
    {
        ReGenerate( mBuildData.generator, mBuildData.seed );
    }
    if( ImGui::DragFloat( "Iso Surface", &mBuildData.isoSurface, 0.1f ) )
    {
        ReGenerate( mBuildData.generator, mBuildData.seed );
    }

    UpdateChunksForPosition( cameraPosition );
}

void MeshNoisePreview::GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue )
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

void MeshNoisePreview::UpdateChunksForPosition( Vector3 position )
{
    const int range = 2;

    position += Vector3( Chunk::SIZE / 2.0f );

    Vector3i chunkCenter = (Vector3i( position ) / Chunk::SIZE) * Chunk::SIZE;

    std::vector<Vector3i> chunkPositions;

    for( int x = -range; x <= range; x++ )
    {
        for( int y = -range; y <= range; y++ )
        {
            for( int z = -range; z <= range; z++ )
            {
                Vector3i chunkPos = Vector3i( x, y, z ) * Chunk::SIZE + chunkCenter;

                if( std::find_if( mChunks.begin(), mChunks.end(), [&]( const Chunk& chunk ) { return chunk.GetPos() == chunkPos; } ) == mChunks.end() &&
                    std::find( mInProgressChunks.begin(), mInProgressChunks.end(), chunkPos ) == mInProgressChunks.end() )
                {
                    chunkPositions.push_back( chunkPos );                    
                }
            }
        }
    }

    std::sort( chunkPositions.begin(), chunkPositions.end(), [chunkCenter]( const Vector3i& a, const Vector3i& b )
        {
            return (chunkCenter - a).dot() < (chunkCenter - b).dot();
        } );

    for( const Vector3i& pos : chunkPositions )
    {
        if( mInProgressChunks.size() >= mThreads.size() * 2 )
        {
            break;
        }

        mBuildData.pos = pos;
        mGenerateQueue.Push( mBuildData );
        mInProgressChunks.insert( pos );
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

    float xLight = abs( dot( LIGHT_DIR.normalized(), Vector3( 1, 0, 0 ) ) ) * (1.0f - AMBIENT_LIGHT) + AMBIENT_LIGHT;
    Color3 colorRight = buildData.color * xLight;
    Color3 colorLeft = buildData.color * (1.0f - xLight);

    float yLight = abs( dot( LIGHT_DIR.normalized(), Vector3( 0, 1, 0 ) ) ) * (1.0f - AMBIENT_LIGHT) + AMBIENT_LIGHT;
    Color3 colorUp = buildData.color * yLight;
    Color3 colorDown = buildData.color * (1.0f - yLight);

    float zLight = abs( dot( LIGHT_DIR.normalized(), Vector3( 0, 0, 1 ) ) ) * (1.0f - AMBIENT_LIGHT) + AMBIENT_LIGHT;
    Color3 colorForward = buildData.color * zLight;
    Color3 colorBack = buildData.color * (1.0f - zLight);

    constexpr int32_t STEP_X = SIZE_GEN * SIZE_GEN;
    constexpr int32_t STEP_Y = SIZE_GEN;
    constexpr int32_t STEP_Z = 1;

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
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_X, STEP_Y, STEP_Z, colorRight,
                            Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ) );
                    }

                    if( densityValues[noiseIdx - STEP_X] < buildData.isoSurface ) // Left
                    {
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_X, -STEP_Y, STEP_Z, colorLeft,
                            Vector3( xf, yf + 1, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                    }

                    if( densityValues[noiseIdx + STEP_Y] < buildData.isoSurface ) // Up
                    {
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_Y, STEP_Z, STEP_X, colorUp,
                            Vector3( xf, yf + 1, zf ), Vector3( xf, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf ) );
                    }

                    if( densityValues[noiseIdx - STEP_Y] < buildData.isoSurface ) // Down
                    {
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_Y, -STEP_Z, STEP_X, colorDown,
                            Vector3( xf, yf, zf + 1 ), Vector3( xf, yf, zf ), Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf, zf + 1 ) );
                    }

                    if( densityValues[noiseIdx + STEP_Z] < buildData.isoSurface ) // Forward
                    {
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx + STEP_Z, STEP_X, STEP_Y, colorForward,
                            Vector3( xf, yf, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                    }

                    if( densityValues[noiseIdx - STEP_Z] < buildData.isoSurface ) // Back
                    {
                        AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx - STEP_Z, -STEP_X, STEP_Y, colorBack,
                            Vector3( xf + 1, yf, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf ) );
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
    mPos = meshData.pos;

    if( !meshData.vertexData.empty() )
    {
        mVertexBuffer = GL::Buffer( GL::Buffer::TargetHint::Array, meshData.vertexData );
        mIndexBuffer  = GL::Buffer( GL::Buffer::TargetHint::Array, meshData.indicies );

        //https://doc.magnum.graphics/magnum/classMagnum_1_1GL_1_1Mesh.html

        mMesh = GL::Mesh( GL::MeshPrimitive::Triangles );

        mMesh.setCount( mIndexBuffer.size() )
            .setIndexBuffer( mIndexBuffer, 0, GL::MeshIndexType::UnsignedInt, 0, mVertexBuffer.size() - 1 )
            .addVertexBuffer( mVertexBuffer, 0, Shaders::VertexColor3D::Position{}, Shaders::VertexColor3D::Color3{} );
    }

    meshData.Free();
}

void MeshNoisePreview::Chunk::AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
    int32_t facingIdx, int32_t offsetA, int32_t offsetB, Color3 color, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 )
{
    uint8_t sideA0 = density[facingIdx - offsetA] >= isoSurface;
    uint8_t sideA1 = density[facingIdx + offsetA] >= isoSurface;
    uint8_t sideB0 = density[facingIdx - offsetB] >= isoSurface;
    uint8_t sideB1 = density[facingIdx + offsetB] >= isoSurface;

    uint8_t corner00 = (sideA0 && sideB0) || density[facingIdx - offsetA - offsetB] >= isoSurface;
    uint8_t corner01 = (sideA0 && sideB1) || density[facingIdx - offsetA + offsetB] >= isoSurface;
    uint8_t corner10 = (sideA1 && sideB0) || density[facingIdx + offsetA - offsetB] >= isoSurface;
    uint8_t corner11 = (sideA1 && sideB1) || density[facingIdx + offsetA + offsetB] >= isoSurface;

    float light00 = (float)(sideA0 + sideB0 + corner00) / 3.0f;
    float light01 = (float)(sideA1 + sideB0 + corner10) / 3.0f;
    float light10 = (float)(sideA0 + sideB1 + corner01) / 3.0f;
    float light11 = (float)(sideA1 + sideB1 + corner11) / 3.0f;

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
        vert.addSource( rs.get( "generic.glsl" ) )
            .addSource( rs.get( "VertexColor3D.vert" ) ).compile() );
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
