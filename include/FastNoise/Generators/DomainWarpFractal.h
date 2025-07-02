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

        MetadataT() : MetadataT<Fractal<DomainWarp>>( { "Domain Warp Source", "Uses the algorithm from this domain warp node for each octave of the fractal" }, false )
        {
            groups.push_back( "Domain Warp" );
            groups.push_back( "Fractal" );

            description =
                "The original input position is passed into the first domain warp octave\n"
                "The warped output position from the previous octave is passed into\n"
                "the next octave's input position and so on for each octave\n"
                "The final position is used to generate the source node on the attached domain warp node";
        }
    };
#endif

    class DomainWarpFractalIndependent : public virtual Fractal<DomainWarp>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpFractalIndependent> : MetadataT<DomainWarpFractalProgressive> // Inherits from DomainWarpFractalProgressive just to avoid duplicate code
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            description =
                "The original input position is passed into all domain warp octaves\n"
                "The warped offset from all octaves is accumulated\n"
                "and added to the original input position\n"
                "This position is used to generate the source node on the attached domain warp node";
        }
    };
#endif
}
