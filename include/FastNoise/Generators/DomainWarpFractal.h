#pragma once
#include "Fractal.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpFractalProgressive : public virtual Fractal<DomainWarp>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpFractalProgressive> : MetadataT<Fractal<DomainWarp>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<Fractal<DomainWarp>>( "Domain Warp Source", false )
        {
            groups.push_back( "Domain Warp" );
            groups.push_back( "Fractal" );
        }
    };
#endif

    class DomainWarpFractalIndependant : public virtual Fractal<DomainWarp>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpFractalIndependant> : MetadataT<Fractal<DomainWarp>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT() : MetadataT<Fractal<DomainWarp>>( "Domain Warp Source", false )
        {
            groups.push_back( "Domain Warp" );
            groups.push_back( "Fractal" );
        }
    };
#endif
}
