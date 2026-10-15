// Instantiates every FastNoise node for the symbolic feature set and traces node trees
// into the IR. See CodegenRegister.h for how the unmodified node code is reused.

// Compile the node code for the symbolic pseudo feature set: FeatureSetDefault() must
// return it so that float32v/int32v in Generator.inl resolve to the recording registers.
// The maximum feature set is pinned to SCALAR so no hardware register headers (and their
// overloads for any "native" register) take part in overload resolution in this unit.
#define FASTSIMD_MAX_FEATURE_SET SCALAR
#define FASTSIMD_DEFAULT_FEATURE_SET Invalid
#define FASTSIMD_FEATURE_VALUE_Invalid() 0

#include <FastSIMD/ToolSet.h>

#include "CodegenRegister.h"
#include "CodegenGenerator.h"

#include <FastNoise/Codegen.h>
#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>

#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace FastNoise::Codegen
{
    struct NodeFactory
    {
        const char* name;
        Generator* ( *create )();
    };

    static std::vector<NodeFactory>& Factories()
    {
        static std::vector<NodeFactory> factories;
        return factories;
    }

    struct NodeFactoryRegistration
    {
        NodeFactoryRegistration( const char* name, Generator* ( *create )() )
        {
            Factories().push_back( { name, create } );
        }
    };
}

// Reuse the canonical node list. Each node is explicitly instantiated for the symbolic
// feature set (this is what compiles the node code against the recording registers) and
// registered in a factory table keyed by class name.
#define FASTNOISE_REGISTER_NODE( CLASS ) \
    template class FastSIMD::DispatchClass<FastNoise::CLASS, FastNoise::Codegen::kFeatureSet>; \
    static const FastNoise::Codegen::NodeFactoryRegistration gCodegenRegister_##CLASS( #CLASS, \
        []() -> FastNoise::Generator* { return new FastSIMD::DispatchClass<FastNoise::CLASS, FastNoise::Codegen::kFeatureSet>(); } )

#include "../FastSIMD_Build.inl"

namespace FastNoise::Codegen
{
    // Owns symbolic node instances built from a NodeData tree
    class NodeSet
    {
    public:
        ~NodeSet()
        {
            // Nodes hold SmartNode references to their sources. Keep every reference count
            // above zero while destroying so SmartNode never tries to free them itself.
            for( Generator* node : mNodes )
            {
                Internal::BumpNodeRefences( node, true );
            }
            for( Generator* node : mNodes )
            {
                delete node;
            }
        }

        Generator* Build( const NodeData* data )
        {
            if( !data || !data->metadata )
            {
                return nullptr;
            }

            auto existing = mBuilt.find( data );
            if( existing != mBuilt.end() )
            {
                return existing->second;
            }

            const Metadata* metadata = data->metadata;
            Generator* node = nullptr;

            for( const NodeFactory& factory : Factories() )
            {
                if( std::strcmp( factory.name, metadata->name ) == 0 )
                {
                    node = factory.create();
                    break;
                }
            }

            if( !node )
            {
                return nullptr;
            }

            mNodes.push_back( node );
            mBuilt.emplace( data, node );

            for( size_t i = 0; i < metadata->memberVariables.size() && i < data->variables.size(); i++ )
            {
                metadata->memberVariables[i].setFunc( node, data->variables[i] );
            }

            for( size_t i = 0; i < metadata->memberNodeLookups.size() && i < data->nodeLookups.size(); i++ )
            {
                Generator* source = Build( data->nodeLookups[i] );

                if( !source || !metadata->memberNodeLookups[i].setFunc( node, Internal::ToSmartNode( source ) ) )
                {
                    return nullptr;
                }
            }

            for( size_t i = 0; i < metadata->memberHybrids.size() && i < data->hybrids.size(); i++ )
            {
                if( data->hybrids[i].first )
                {
                    Generator* source = Build( data->hybrids[i].first );

                    if( !source || !metadata->memberHybrids[i].setNodeFunc( node, Internal::ToSmartNode( source ) ) )
                    {
                        return nullptr;
                    }
                }
                else
                {
                    metadata->memberHybrids[i].setValueFunc( node, data->hybrids[i].second );
                }
            }

            return node;
        }

    private:
        std::vector<Generator*> mNodes;
        std::unordered_map<const NodeData*, Generator*> mBuilt;
    };

    // Traces one Function per (node, dimension count), discovering callees on the way
    class TracerImpl final : public Tracer
    {
    public:
        explicit TracerImpl( Module& module ) : mModule( module ) {}

        std::int32_t RequestFunction( const Generator* node, int dimensions ) override
        {
            auto key = std::make_pair( node, dimensions );
            auto found = mFunctions.find( key );

            if( found != mFunctions.end() )
            {
                return found->second;
            }

            auto nodeIdx = mNodeIds.emplace( node, (int)mNodeIds.size() ).first->second;

            Function function;
            function.node = node;
            function.dimensions = dimensions;
            function.name = "fn_" + std::string( node->GetMetadata().name ) + "_" + std::to_string( nodeIdx ) + "_" + std::to_string( dimensions ) + "D";

            std::int32_t idx = (std::int32_t)mModule.functions.size();
            mModule.functions.push_back( std::move( function ) );
            mFunctions.emplace( key, idx );
            mWorklist.push_back( idx );
            return idx;
        }

        void Run()
        {
            while( !mWorklist.empty() )
            {
                std::int32_t idx = mWorklist.back();
                mWorklist.pop_back();
                Trace( idx );
            }
        }

    private:
        void Trace( std::int32_t idx )
        {
            // Trace into a local so that functions requested during tracing can grow the module
            Function function = std::move( mModule.functions[idx] );
            Builder builder( function );

            Context previous = gContext;
            gContext = { &builder, this };

            auto* gen = dynamic_cast<const FastSIMD::DispatchClass<Generator, kFeatureSet>*>( function.node );
            assert( gen );

            int32v seed( FromId{}, builder.Leaf( Op::Param, Type::I32, 0 ) );
            float32v pos[4];
            for( int i = 0; i < function.dimensions; i++ )
            {
                pos[i] = float32v( FromId{}, builder.Leaf( Op::Param, Type::F32, i + 1 ) );
            }

            float32v result;
            switch( function.dimensions )
            {
            case 2: result = gen->Gen( seed, pos[0], pos[1] ); break;
            case 3: result = gen->Gen( seed, pos[0], pos[1], pos[2] ); break;
            default: result = gen->Gen( seed, pos[0], pos[1], pos[2], pos[3] ); break;
            }
            function.result = result.Id();

            gContext = previous;
            mModule.functions[idx] = std::move( function );
        }

        Module& mModule;
        std::map<std::pair<const Generator*, int>, std::int32_t> mFunctions;
        std::unordered_map<const Generator*, int> mNodeIds;
        std::vector<std::int32_t> mWorklist;
    };

    std::string Generate( const NodeData& root, const Options& options )
    {
        if( options.dimensions < 2 || options.dimensions > 4 )
        {
            return "";
        }

        NodeSet nodes;
        Generator* rootNode = nodes.Build( &root );

        if( !rootNode )
        {
            return "";
        }

        Module module;
        TracerImpl tracer( module );
        module.entry = tracer.RequestFunction( rootNode, options.dimensions );
        tracer.Run();

        return EmitModule( module, options );
    }

    std::string Generate( const char* encodedNodeTree, const Options& options )
    {
        std::vector<std::unique_ptr<NodeData>> storage;
        NodeData* root = Metadata::DeserialiseNodeData( encodedNodeTree, storage );

        if( !root )
        {
            return "";
        }

        return Generate( *root, options );
    }
}
