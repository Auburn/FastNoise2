#pragma once
#include "Generator.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpSimplex : public virtual DomainWarp
    {
    public:        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpSimplex> : MetadataT<DomainWarp>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
    };
#endif
}
