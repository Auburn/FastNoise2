#pragma once
#include "Generator.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpOpenSimplex : public virtual DomainWarp
    {
    public:
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpOpenSimplex> : MetadataT<DomainWarp>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
    };
#endif
}