#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>
#include <FastNoise/CodeGen.h>

#include "TraceToolSet.h"
#include "Emit.h"

// Compiles every node's SIMD implementation against the symbolic trace registers (see TraceToolSet.h)
// Running Gen() on a trace node then records the node's operations, with all its settings baked in as constants

using float32v = FS::TraceFloat32;
using int32v = FS::TraceInt32;
using mask32v = FS::TraceMask32;

namespace FastNoise
{
    namespace CodeGen
    {
        using TraceNode = FastSIMD::DispatchClass<Generator, FastSIMD::kTraceFeatureSet>;

        template<typename... P>
        float32v TraceSourceCall( const TraceNode* node, int32v seed, P... pos );

        template<typename GENERIC, typename... P>
        float32v TraceNativeHelper( const char* native, const GENERIC& generic, int32v hash, P... pos );
    }
}

// Replaces the generic DispatchClass<Generator, SIMD> from Generator.inl for the trace feature set,
// source lookups record a call to the source node instead of a virtual call
template<>
class FastSIMD::DispatchClass<FastNoise::Generator, FastSIMD::kTraceFeatureSet> : public virtual FastNoise::Generator
{
public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v /*w*/ ) const { return Gen( seed, x, y, z ); }

    FastSIMD::FeatureSet GetActiveFeatureSet() const final
    {
        return FastSIMD::kTraceFeatureSet;
    }

    using VoidPtrStorageType = const DispatchClass*;

    void SetSourceSIMDPtr( const FastNoise::Generator* base, const void** simdPtr ) final
    {
        VoidPtrStorageType simd = base ? dynamic_cast<VoidPtrStorageType>( base ) : nullptr;

        assert( !base || simd );
        *simdPtr = simd;
    }

    template<typename T, typename... POS>
    static float32v GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        if( memberVariable.simdGeneratorPtr )
        {
            return FastNoise::CodeGen::TraceSourceCall( reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr ), seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    static float32v GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos )
    {
        assert( memberVariable.simdGeneratorPtr );
        return FastNoise::CodeGen::TraceSourceCall( reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr ), seed, pos... );
    }

    template<typename T>
    static const DispatchClass<T, FastSIMD::kTraceFeatureSet>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable )
    {
        assert( memberVariable.simdGeneratorPtr );
        return static_cast<const DispatchClass<T, FastSIMD::kTraceFeatureSet>*>( reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr ) );
    }

    // Trace nodes only exist to be traced, they never generate values
    FastNoise::OutputMinMax GenUniformGrid2D( float*, float, float, int, int, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenUniformGrid3D( float*, float, float, float, int, int, int, float, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenUniformGrid4D( float*, float, float, float, float, int, int, int, int, float, float, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenTileable2D( float*, int, int, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenPositionArray2D( float*, int, const float*, const float*, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenPositionArray3D( float*, int, const float*, const float*, const float*, float, float, float, int ) const final { return {}; }
    FastNoise::OutputMinMax GenPositionArray4D( float*, int, const float*, const float*, const float*, const float*, float, float, float, float, int ) const final { return {}; }
    float GenSingle2D( float, float, int ) const final { return 0; }
    float GenSingle3D( float, float, float, int ) const final { return 0; }
    float GenSingle4D( float, float, float, float, int ) const final { return 0; }

private:
    // Trace nodes are owned by their TraceSession, SmartNode references to them must never free them
    int32_t ReferencesFetchAdd( int32_t add ) const noexcept final
    {
        return mReferences.fetch_add( add, std::memory_order_relaxed ) + 2;
    }

    mutable std::atomic<int32_t> mReferences = 0;
};

namespace FastNoise
{
    namespace CodeGen
    {
        struct TraceNodeFactory
        {
            using CreateFunc = Generator* (*)();

            TraceNodeFactory( const char* name, CreateFunc create )
            {
                GetAll().emplace_back( name, create );
            }

            static std::vector<std::pair<const char*, CreateFunc>>& GetAll()
            {
                static std::vector<std::pair<const char*, CreateFunc>> sFactories;
                return sFactories;
            }

            static Generator* Create( const char* name )
            {
                for( const auto& factory : GetAll() )
                {
                    if( std::strcmp( factory.first, name ) == 0 )
                    {
                        return factory.second();
                    }
                }
                return nullptr;
            }
        };

        class TraceSession
        {
        public:
            explicit TraceSession( const Options& options ) :
                mOptions( options ) { }

            TraceSession( const TraceSession& ) = delete;

            ~TraceSession()
            {
                // Parents release their sources on destruction, so delete in reverse post order
                for( size_t i = mPostOrder.size(); i-- > 0; )
                {
                    delete mPostOrder[i];
                }
            }

            static TraceSession& Current()
            {
                assert( sCurrent );
                return *sCurrent;
            }

            const TraceNode* Build( const NodeData* nodeData, std::string& error )
            {
                Generator* generator = BuildNode( nodeData, error );

                return generator ? dynamic_cast<const TraceNode*>( generator ) : nullptr;
            }

            void Run( const TraceNode* root, IR::Program& program )
            {
                CurrentScope scope( this );

                mInline.clear();

                if( !mOptions.inlineAll )
                {
                    TracePass( root );
                    DecideInlining();
                }
                TracePass( root );

                program.functions.assign( std::make_move_iterator( mFunctions.begin() ), std::make_move_iterator( mFunctions.end() ) );
                IR::Compact( program );
            }

            bool ShouldInline( const TraceNode* node, int dimensions ) const
            {
                return mOptions.inlineAll || mInline.count( { node, dimensions } );
            }

            int GetFunction( const TraceNode* node, int dimensions )
            {
                auto key = std::make_pair( node, dimensions );
                auto find = mFunctionLookup.find( key );

                if( find != mFunctionLookup.end() )
                {
                    return find->second;
                }

                int index = (int)mFunctions.size();
                IR::Function& function = mFunctions.emplace_back();
                mFunctionNodes.push_back( node );
                mFunctionLookup.emplace( key, index );

                function.name = node->GetMetadata().name;
                function.params.push_back( { IR::Type::I32, "seed" } );
                for( int i = 0; i < dimensions; i++ )
                {
                    function.params.push_back( { IR::Type::F32, std::string( 1, "xyzw"[i] ) } );
                }
                return index;
            }

            // Helpers get their own function, traced when first called in each pass
            template<typename GENERIC, typename... P>
            int GetHelper( const char* native, const GENERIC& generic )
            {
                std::string key = native + std::to_string( sizeof...( P ) );
                auto find = mHelperLookup.find( key );

                if( find != mHelperLookup.end() )
                {
                    return find->second;
                }

                int index = (int)mFunctions.size();
                IR::Function& function = mFunctions.emplace_back();
                mFunctionNodes.push_back( nullptr );
                mHelperLookup.emplace( key, index );

                const char* name = std::strrchr( native, ':' );
                function.name = name ? name + 1 : native;
                function.native = native;
                function.params.push_back( { IR::Type::I32, "hash" } );
                for( size_t i = 0; i < sizeof...( P ); i++ )
                {
                    function.params.push_back( { IR::Type::F32, std::string( 1, "xyzw"[i] ) } );
                }

                IR::Builder builder( function, mOptions.fastMath );
                TraceHelper<P...>( function, builder, generic, std::index_sequence_for<P...>() );
                return index;
            }

        private:
            // Functions small enough that inlining them at every call site is better than a call
            static constexpr size_t kSmallFunctionOps = 12;

            struct CurrentScope
            {
                CurrentScope( TraceSession* session ) : previous( sCurrent ) { sCurrent = session; }
                ~CurrentScope() { sCurrent = previous; }
                TraceSession* previous;
            };

            Generator* BuildNode( const NodeData* nodeData, std::string& error )
            {
                if( !nodeData || !nodeData->metadata )
                {
                    error = "Invalid node data";
                    return nullptr;
                }

                auto find = mBuilt.find( nodeData );
                if( find != mBuilt.end() )
                {
                    if( !find->second )
                    {
                        error = "Node tree contains a dependency loop";
                    }
                    return find->second;
                }

                const Metadata* metadata = nodeData->metadata;
                Generator* generator = TraceNodeFactory::Create( metadata->name );

                if( !generator )
                {
                    error = std::string( "Node type does not support code generation: " ) + metadata->name;
                    return nullptr;
                }

                mBuilt.emplace( nodeData, nullptr );

                bool success = Configure( generator, nodeData, error );

                mPostOrder.push_back( generator );

                if( !success )
                {
                    return nullptr;
                }

                mBuilt[nodeData] = generator;
                return generator;
            }

            Generator* BuildConstant()
            {
                Generator* constant = TraceNodeFactory::Create( "Constant" );
                mPostOrder.push_back( constant );
                return constant;
            }

            // Mirrors decoding in Metadata.cpp so the trace node matches what NewFromEncodedNodeTree() creates
            bool Configure( Generator* generator, const NodeData* nodeData, std::string& error )
            {
                const Metadata* metadata = nodeData->metadata;

                for( size_t i = 0; i < metadata->memberVariables.size() && i < nodeData->variables.size(); i++ )
                {
                    metadata->memberVariables[i].setFunc( generator, nodeData->variables[i] );
                }

                for( size_t i = 0; i < metadata->memberNodeLookups.size(); i++ )
                {
                    const NodeData* sourceData = i < nodeData->nodeLookups.size() ? nodeData->nodeLookups[i] : nullptr;
                    Generator* source = sourceData ? BuildNode( sourceData, error ) : BuildConstant();

                    if( !source )
                    {
                        return false;
                    }

                    if( !metadata->memberNodeLookups[i].setFunc( generator, Internal::ToSmartNode( source ) ) )
                    {
                        error = std::string( "Invalid source node type for " ) + metadata->name + "::" + metadata->memberNodeLookups[i].name;
                        return false;
                    }
                }

                for( size_t i = 0; i < metadata->memberHybrids.size() && i < nodeData->hybrids.size(); i++ )
                {
                    if( const NodeData* sourceData = nodeData->hybrids[i].first )
                    {
                        Generator* source = BuildNode( sourceData, error );

                        if( !source )
                        {
                            return false;
                        }

                        if( !metadata->memberHybrids[i].setNodeFunc( generator, Internal::ToSmartNode( source ) ) )
                        {
                            error = std::string( "Invalid source node type for " ) + metadata->name + "::" + metadata->memberHybrids[i].name;
                            return false;
                        }
                    }
                    else
                    {
                        metadata->memberHybrids[i].setValueFunc( generator, nodeData->hybrids[i].second );
                    }
                }
                return true;
            }

            void TracePass( const TraceNode* root )
            {
                mFunctions.clear();
                mFunctionNodes.clear();
                mFunctionLookup.clear();
                mHelperLookup.clear();

                GetFunction( root, mOptions.dimensions );

                // Calls append functions while tracing, deque keeps references stable
                for( size_t i = 0; i < mFunctions.size(); i++ )
                {
                    // Helpers are already traced
                    if( mFunctionNodes[i] )
                    {
                        TraceFunction( (int)i );
                    }
                }
            }

            void TraceFunction( int index )
            {
                IR::Function& function = mFunctions[index];
                const TraceNode* node = mFunctionNodes[index];

                IR::Builder builder( function, mOptions.fastMath );

                int32v seed( builder.Param( 0, IR::Type::I32 ), {} );
                float32v x( builder.Param( 1, IR::Type::F32 ), {} );
                float32v y( builder.Param( 2, IR::Type::F32 ), {} );

                switch( function.params.size() - 1 )
                {
                case 2:
                    function.result = node->Gen( seed, x, y ).id;
                    break;
                case 3:
                    function.result = node->Gen( seed, x, y, float32v( builder.Param( 3, IR::Type::F32 ), {} ) ).id;
                    break;
                default:
                    function.result = node->Gen( seed, x, y, float32v( builder.Param( 3, IR::Type::F32 ), {} ), float32v( builder.Param( 4, IR::Type::F32 ), {} ) ).id;
                    break;
                }
            }

            template<typename... P, typename GENERIC, size_t... I>
            static void TraceHelper( IR::Function& function, IR::Builder& builder, const GENERIC& generic, std::index_sequence<I...> )
            {
                int32v hash( builder.Param( 0, IR::Type::I32 ), {} );
                IR::ValueId pos[] = { builder.Param( (int)I + 1, IR::Type::F32 )... };

                function.result = generic( hash, P( pos[I], {} )... ).id;
            }

            // Inline node functions with a single call site in the final program, or that are small enough
            void DecideInlining()
            {
                size_t count = mFunctions.size();
                std::vector<std::map<int, int>> calls( count );
                std::vector<size_t> ops( count );

                for( size_t f = 0; f < count; f++ )
                {
                    std::vector<bool> live = IR::FindLive( mFunctions[f] );

                    for( size_t i = 0; i < live.size(); i++ )
                    {
                        if( live[i] && mFunctions[f].code[i].op == IR::Op::Call )
                        {
                            calls[f][mFunctions[f].code[i].imm]++;
                        }
                    }
                    ops[f] = IR::CountOps( mFunctions[f] );
                }

                // Callers before callees, the call graph is acyclic
                std::vector<int> order;
                std::vector<bool> visited( count, false );

                auto visit = [&]( auto& self, int f ) -> void
                {
                    visited[f] = true;
                    for( const auto& call : calls[f] )
                    {
                        if( !visited[call.first] )
                        {
                            self( self, call.first );
                        }
                    }
                    order.push_back( f );
                };
                visit( visit, 0 );

                // Copies of each function body once inlining decisions above it are applied
                std::vector<int> callSites( count, 0 );

                for( size_t o = order.size(); o-- > 0; )
                {
                    int f = order[o];

                    // Helpers stay functions so C++ output can call the native implementation
                    if( !mFunctionNodes[f] )
                    {
                        continue;
                    }

                    bool inlined = f != 0 && ( callSites[f] <= 1 || ops[f] <= kSmallFunctionOps );
                    int bodies = inlined ? callSites[f] : 1;

                    if( inlined )
                    {
                        mInline.insert( { mFunctionNodes[f], (int)mFunctions[f].params.size() - 1 } );
                    }

                    for( const auto& call : calls[f] )
                    {
                        callSites[call.first] += call.second * bodies;
                    }
                }
            }

            static inline thread_local TraceSession* sCurrent = nullptr;

            Options mOptions;
            std::unordered_map<const NodeData*, Generator*> mBuilt;
            std::vector<Generator*> mPostOrder;

            std::deque<IR::Function> mFunctions;
            std::vector<const TraceNode*> mFunctionNodes; // nullptr for helpers
            std::map<std::pair<const TraceNode*, int>, int> mFunctionLookup;
            std::unordered_map<std::string, int> mHelperLookup;
            std::set<std::pair<const TraceNode*, int>> mInline;
        };

        template<typename... P>
        float32v TraceSourceCall( const TraceNode* node, int32v seed, P... pos )
        {
            TraceSession& session = TraceSession::Current();
            constexpr int kDimensions = (int)sizeof...( P );

            if( session.ShouldInline( node, kDimensions ) )
            {
                return node->Gen( seed, pos... );
            }

            IR::ValueId args[] = { seed.id, pos.id... };

            return float32v( IR::Builder::Current().EmitCall( session.GetFunction( node, kDimensions ), args, kDimensions + 1 ), {} );
        }

        // Records a call to a helper instead of tracing it inline, generic is the helper's generic implementation
        template<typename GENERIC, typename... P>
        float32v TraceNativeHelper( const char* native, const GENERIC& generic, int32v hash, P... pos )
        {
            static_assert( ( std::is_same_v<P, float32v> && ... ), "Helpers take a hash followed by positions" );

            int function = TraceSession::Current().GetHelper<GENERIC, P...>( native, generic );
            IR::ValueId args[] = { hash.id, pos.id... };

            return float32v( IR::Builder::Current().EmitCall( function, args, (int)sizeof...( P ) + 1 ), {} );
        }
    }
}

// Helpers with feature set specific implementations (AVX2/AVX512 permutes),
// C++ output calls these natively instead of the generic path that is traced for other languages
#include <FastNoise/Generators/Utils.inl>

namespace FastNoise
{
    template<>
    float32v GetGradientDotPerlin<FastSIMD::kTraceFeatureSet>( int32v hash, float32v fX, float32v fY )
    {
        return CodeGen::TraceNativeHelper( "FastNoise::GetGradientDotPerlin", []( auto... args ) { return GetGradientDotPerlin<FastSIMD::kTraceGenericFeatureSet>( args... ); }, hash, fX, fY );
    }

    template<>
    float32v GetGradientDotPerlin<FastSIMD::kTraceFeatureSet>( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW )
    {
        return CodeGen::TraceNativeHelper( "FastNoise::GetGradientDotPerlin", []( auto... args ) { return GetGradientDotPerlin<FastSIMD::kTraceGenericFeatureSet>( args... ); }, hash, fX, fY, fZ, fW );
    }

    template<>
    float32v GetGradientDotCommon<FastSIMD::kTraceFeatureSet>( int32v hash, float32v fX, float32v fY, float32v fZ )
    {
        return CodeGen::TraceNativeHelper( "FastNoise::GetGradientDotCommon", []( auto... args ) { return GetGradientDotCommon<FastSIMD::kTraceGenericFeatureSet>( args... ); }, hash, fX, fY, fZ );
    }
}

#define FASTNOISE_REGISTER_NODE( CLASS ) \
static const FastNoise::CodeGen::TraceNodeFactory g ## CLASS ## TraceNodeFactory( #CLASS, []() -> FastNoise::Generator* { return new FastSIMD::DispatchClass<FastNoise::CLASS, FastSIMD::kTraceFeatureSet>; } )

#include "../FastSIMD_Build.inl"

namespace FastNoise
{
    namespace CodeGen
    {
        struct Program::Impl
        {
            Options options;
            IR::Program program;
        };

        Program::Program( Impl* impl ) :
            mImpl( impl ) { }

        Program::~Program()
        {
            delete mImpl;
        }

        std::unique_ptr<Program> Program::FromEncodedNodeTree( const char* encodedNodeTree, const Options& options, std::string* error )
        {
            std::vector<std::unique_ptr<NodeData>> nodeDataStorage;
            const NodeData* root = encodedNodeTree ? Metadata::DeserialiseNodeData( encodedNodeTree, nodeDataStorage ) : nullptr;

            if( !root )
            {
                if( error )
                {
                    *error = "Could not decode node tree";
                }
                return nullptr;
            }

            return FromNodeData( root, options, error );
        }

        std::unique_ptr<Program> Program::FromNodeData( const NodeData* nodeData, const Options& options, std::string* error )
        {
            std::string traceError;

            if( options.dimensions < 2 || options.dimensions > 4 )
            {
                traceError = "Dimensions must be 2, 3 or 4";
            }
            else
            {
                TraceSession session( options );

                if( const TraceNode* root = session.Build( nodeData, traceError ) )
                {
                    auto impl = std::make_unique<Impl>();
                    impl->options = options;
                    session.Run( root, impl->program );

                    return std::unique_ptr<Program>( new Program( impl.release() ) );
                }
            }

            if( error )
            {
                *error = traceError;
            }
            return nullptr;
        }

        std::string Program::Emit( Language language ) const
        {
            return EmitProgram( mImpl->program, language, mImpl->options.functionName );
        }

        float Program::Evaluate( const float* positions, int seed ) const
        {
            return IR::Evaluate( mImpl->program, seed, positions );
        }

        int Program::GetDimensions() const
        {
            return mImpl->options.dimensions;
        }

        int Program::GetFunctionCount() const
        {
            return (int)mImpl->program.functions.size();
        }

        size_t Program::GetInstructionCount() const
        {
            size_t count = 0;
            for( const IR::Function& function : mImpl->program.functions )
            {
                count += IR::CountOps( function );
            }
            return count;
        }

        std::string Generate( const char* encodedNodeTree, Language language, const Options& options )
        {
            if( auto program = Program::FromEncodedNodeTree( encodedNodeTree, options ) )
            {
                return program->Emit( language );
            }
            return {};
        }
    }
}
