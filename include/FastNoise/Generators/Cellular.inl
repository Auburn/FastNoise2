#include <cfloat>
#include <array>
#include <type_traits>

#include "Cellular.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD, typename PARENT>
class FastSIMD::DispatchClass<Cellular<PARENT>, SIMD> : public virtual Cellular<PARENT>, public DispatchClass<PARENT, SIMD>
{
protected:
    static constexpr float kJitter2D = 0.437016f;
    static constexpr float kJitter3D = 0.396144f;
    static constexpr float kJitter4D = 0.366025f;
    static constexpr float kJitterIdx23 = 0.190983f;

    template<DistanceFunction DIST_FUNC>
    using DistanceFunctionConstant = std::integral_constant<DistanceFunction, DIST_FUNC>;

    template<int COUNT>
    using CountConstant = std::integral_constant<int, COUNT>;

    // Picks the GenT variant by calling func( specialised, distanceFunction, count, sizeJitter ) with compile time constants
    // Common settings get a cell loop specialised on distance function, number of tracked cells (count) and size jitter
    // Everything else uses the generic variant: specialised = false
    // Euclidean finds the same closest cells as EuclideanSquared, so it shares those variants,
    // except with size jitter when the cell loop compares sqrt distances (SQRT_DISTANCE, 2D CellularValue)
    template<int MAX_COUNT, bool SQRT_DISTANCE = false, typename FUNC>
    FS_FORCEINLINE auto SelectGenT( int count, FUNC&& func ) const
    {
        bool sizeJitterActive = this->mSizeJitter.simdGeneratorPtr || this->mSizeJitter.constant != 0.0f;

        switch( this->mDistanceFunction )
        {
        case DistanceFunction::Euclidean:
            if( SQRT_DISTANCE && sizeJitterActive )
            {
                break;
            }
        case DistanceFunction::EuclideanSquared:
            if( !sizeJitterActive )
            {
                return SelectCount<DistanceFunction::EuclideanSquared, MAX_COUNT>( count, func );
            }
            if( count == 1 ) // Size jitter is less common, only specialise the single closest cell case
            {
                return func( std::true_type{}, DistanceFunctionConstant<DistanceFunction::EuclideanSquared>{}, CountConstant<1>{}, std::true_type{} );
            }
            break;
        case DistanceFunction::Manhattan:
            if( !sizeJitterActive )
            {
                return SelectCount<DistanceFunction::Manhattan, MAX_COUNT>( count, func );
            }
            break;
        default: // Less common distance functions use the generic variant to limit code size, Minkowski is dominated by Pow()
            break;
        }

        return func( std::false_type{}, DistanceFunctionConstant<DistanceFunction::EuclideanSquared>{}, CountConstant<MAX_COUNT>{}, std::false_type{} );
    }

    template<DistanceFunction DIST_FUNC, int MAX_COUNT, typename FUNC>
    static FS_FORCEINLINE auto SelectCount( int count, FUNC& func )
    {
        constexpr DistanceFunctionConstant<DIST_FUNC> distFunc;

        if constexpr( MAX_COUNT > 1 )
        {
            switch( count )
            {
            case 1:
                return func( std::true_type{}, distFunc, CountConstant<1>{}, std::false_type{} );
            case 2:
                return func( std::true_type{}, distFunc, CountConstant<2>{}, std::false_type{} );
            case 3:
                return func( std::true_type{}, distFunc, CountConstant<3>{}, std::false_type{} );
            }
        }
        return func( std::true_type{}, distFunc, CountConstant<MAX_COUNT>{}, std::false_type{} );
    }

    // Neighbour cell visit orders
    // Nearest: centre first, then by number of off-centre axes (faces, edges, corners) so the distance to beat shrinks early.
    // Shells group the cells that are tested together, see CellIterator::ShellVisitMask
    // Per axis index 1 is the centre cell, 0 and 2 the sides: [-1, +1], or [near, far] per lane after NearFirst()
    // Linear: x-major over [-1, 0, +1], the original order, used when not pruning
    template<int D>
    struct CellOrder
    {
        static constexpr int kCount = D == 2 ? 9 : ( D == 3 ? 27 : 81 );

        // 2D/3D: shell 0 also holds the near side faces (per lane, see NearFirst), visited untested: they can rarely be skipped and
        // testing them would wait on the centre cell's distance. In 4D they are skipped often enough to be worth testing
        static constexpr bool kNearFacesFirst = D <= 3;
        static constexpr uint32_t kFirstShellMask = kNearFacesFirst ? ( 1u << ( D + 1 ) ) - 1 : 1;
        int8_t nearest[kCount][D];
        int8_t linear[kCount][D];
        int8_t pairIndex[kCount][2]; // 4D only: CellBounds pair indices for nearest order
        uint32_t parentMask[kCount]; // Nearest order: bits of the previous shell cells one step closer to the centre
        int shellStart[D + 2];

        constexpr CellOrder() : nearest{}, linear{}, pairIndex{}, parentMask{}, shellStart{}
        {
            int n = 0;
            for( int shell = 0; shell <= D; shell++ )
            {
                shellStart[shell] = n;
                for( int offCentreOrder = 0; offCentreOrder <= D; offCentreOrder++ )
                {
                    for( int i = 0; i < kCount; i++ )
                    {
                        int offCentre = 0, nearSide = 0;
                        for( int d = D - 1, c = i; d >= 0; d--, c /= 3 )
                        {
                            offCentre += c % 3 != 1;
                            nearSide += c % 3 == 0;
                            linear[i][d] = (int8_t)( c % 3 );
                        }
                        int cellShell = kNearFacesFirst && offCentre == 1 && nearSide == 1 ? 0 : offCentre;
                        if( cellShell == shell && offCentre == offCentreOrder )
                        {
                            for( int d = D - 1, c = i; d >= 0; d--, c /= 3 )
                            {
                                nearest[n][d] = (int8_t)( c % 3 );
                            }
                            if constexpr( D == 4 )
                            {
                                pairIndex[n][0] = (int8_t)( nearest[n][0] * 3 + nearest[n][1] );
                                pairIndex[n][1] = (int8_t)( nearest[n][2] * 3 + nearest[n][3] );
                            }
                            n++;
                        }
                    }
                }
            }
            shellStart[D + 1] = n;

            for( int shell = 2; shell <= D; shell++ )
            {
                for( int c = shellStart[shell]; c < shellStart[shell + 1]; c++ )
                {
                    for( int d = 0; d < D; d++ )
                    {
                        if( nearest[c][d] == 1 )
                        {
                            continue;
                        }
                        for( int p = shellStart[shell - 1]; p < shellStart[shell]; p++ )
                        {
                            bool parent = true;
                            for( int e = 0; e < D; e++ )
                            {
                                parent &= nearest[p][e] == ( e == d ? 1 : nearest[c][e] );
                            }
                            if( parent )
                            {
                                parentMask[c] |= 1u << ( p - shellStart[shell - 1] );
                            }
                        }
                    }
                }
            }
        }
    };

    // Per-lane conservative lower bound terms on the distance to each neighbour cell's feature point, see CellIterator
    // Kept apart from CellIterator: the arrays are indexed dynamically so live in memory, the iterator's state can stay in registers
    template<int D>
    struct CellBounds
    {
        std::array<float32v, 3> axis[D];
        float32v pairA[D == 4 ? 9 : 1]; // 4D only: axes 0, 1 precombined
        float32v pairB[D == 4 ? 9 : 1]; // 4D only: axes 2, 3 precombined
    };

    enum PruneMode
    {
        kPruneNone,
        kPruneCoherent, // 4D lanes close together, the same cells are skippable in every lane
        kPruneNearFirst // Incoherent lanes (and all 2D/3D), cells are ordered per lane relative to the near side (see NearFirst)
    };

    // 2D/3D always order cells near side first, see CellOrder::kNearFacesFirst
    // Minkowski is only pruned with P in [0.25, 8] in every lane: other P values make Pow's error/range unreliable
    // and exact distance ties common, which the pruned cell order would resolve differently.
    // Incoherent lanes always prune in 4D, in 2D/3D only when they need the nearest 3 points with grid jitter <= 1,
    // the far side cells then rarely matter in any lane
    template<int D>
    FS_FORCEINLINE static PruneMode GetPruneMode( DistanceFunction distFunc, float32v minkowskiP, const float32v* pos, float32v jitter, float maxJitter, int distanceIndex )
    {
        if( distFunc == DistanceFunction::Minkowski && !AllLanes( FS::Abs( minkowskiP - float32v( 4.125f ) ) <= float32v( 3.875f ) ) )
        {
            return kPruneNone;
        }
        mask32v coherent = FS::Abs( pos[0] - float32v( FS::Extract0( pos[0] ) ) ) <= float32v( 1 );
        for( int d = 1; d < D; d++ )
        {
            coherent &= FS::Abs( pos[d] - float32v( FS::Extract0( pos[d] ) ) ) <= float32v( 1 );
        }
        if( AllLanes( coherent ) )
        {
            return CellOrder<D>::kNearFacesFirst ? kPruneNearFirst : kPruneCoherent;
        }
        if( D == 4 || ( distanceIndex <= 2 && AllLanes( FS::Abs( jitter ) <= float32v( maxJitter * 1.001f ) ) ) )
        {
            return kPruneNearFirst;
        }
        return kPruneNone;
    }

    // Constant indices only, so distance arrays can stay in registers
    template<size_t N>
    FS_FORCEINLINE static float32v DistanceAt( const std::array<float32v, N>& distance, int index )
    {
        float32v value = distance[0];
        for( size_t i = 1; i < N; i++ )
        {
            if( index == (int)i )
            {
                value = distance[i];
            }
        }
        return value;
    }

    FS_FORCEINLINE static bool AllLanes( mask32v m )
    {
        return FS::BitMask( m ) == (FS::BitStorage<mask32v::ElementCount>)( ~0ull >> ( 64 - mask32v::ElementCount ) );
    }

    // Primed cell coordinates and cell positions ( - origin ) for the 3 neighbour cells on one axis: [-1, 0, +1]
    // The hash seed can be xored into one axis' primed coordinates up front, saving an xor per cell
    FS_FORCEINLINE static void CellAxis( int32v cell, int prime, float32v origin, std::array<int32v, 3>& primed, std::array<float32v, 3>& cellPos, int32v seed = int32v( 0 ) )
    {
        float32v cellf = FS::Convert<float>( cell ) - origin;
        cell *= int32v( prime );

        for( int i = 0; i < 3; i++ )
        {
            primed[i] = cell ^ seed;
            cellPos[i] = cellf;
            cellf += float32v( 1 );
            cell += int32v( prime );
        }
    }

    // Reorders an axis' cells per lane to [near side, centre, far side], so pruning sees the same layout in every lane
    template<int D>
    FS_FORCEINLINE static void NearFirst( std::array<int32v, 3>* primed, std::array<float32v, 3>* cellPos, const float32v* pos )
    {
        for( int d = 0; d < D; d++ )
        {
            mask32v up = pos[d] >= FS::Round( pos[d] );

            int32v primedNear = FS::Select( up, primed[d][2], primed[d][0] );
            primed[d][2] = FS::Select( up, primed[d][0], primed[d][2] );
            primed[d][0] = primedNear;

            float32v cellPosNear = FS::Select( up, cellPos[d][2], cellPos[d][0] );
            cellPos[d][2] = FS::Select( up, cellPos[d][0], cellPos[d][2] );
            cellPos[d][0] = cellPosNear;
        }
    }

    // Visits the neighbour cells: while( const int8_t* o = cells.Next( distToBeat ) )
    // o[d] is the cell's slot on axis d in the CellAxis() arrays, cellPos holds cell centres relative to the position if relative is set.
    // When pruning, cells are visited nearest-first and skipped if they can't beat distToBeat in any lane
    template<int D>
    class CellIterator
    {
    public:
        FS_FORCEINLINE CellIterator( CellBounds<D>& bounds, std::array<int32v, 3>* cellPrimed, std::array<float32v, 3>* cellPos, bool relative, const float32v* pos,
                                     DistanceFunction distFunc, float32v minkowskiP, int distanceIndex, float32v jitter, bool sizeJitterActive, const float32v& sizeJitter ) :
            mBounds( bounds )
        {
            PruneMode pruneMode = GetPruneMode<D>( distFunc, minkowskiP, pos, jitter, D == 2 ? kJitter2D : ( D == 3 ? kJitter3D : kJitter4D ), distanceIndex );
            mPrune = pruneMode != kPruneNone;
            mCombineMax = distFunc == DistanceFunction::MaxAxis || distFunc == DistanceFunction::Minkowski;

            // Euclidean: the feature point is within a ball around the cell centre, much tighter than per axis bounds for edge/corner cells
            mBall = distFunc == DistanceFunction::Euclidean || distFunc == DistanceFunction::EuclideanSquared;

            if( mPrune )
            {
                bool nearFirst = pruneMode == kPruneNearFirst;

                // 4D setup is large and amortised over 81 cells: kept out of line so it isn't duplicated into every GenT variant
                if constexpr( D == 4 )
                {
                    mBallRadius = InitPruningNoInline( bounds, cellPrimed, cellPos, relative, pos, nearFirst, distFunc, mBall, mCombineMax, jitter, sizeJitterActive, sizeJitter );
                }
                else
                {
                    mBallRadius = InitPruning( bounds, cellPrimed, cellPos, relative, pos, nearFirst, distFunc, mBall, mCombineMax, jitter, sizeJitterActive, sizeJitter );
                }
            }
        }

        // Next cell's per axis slots, nullptr when done
        FS_FORCEINLINE const int8_t* Next( float32v distToBeat )
        {
            if( !mPrune )
            {
                return mCell < kOrder.kCount ? kOrder.linear[mCell++] : nullptr;
            }

            // Current shell done, test the next one. Separate loop so the threshold maths only runs here
            while( !mVisit )
            {
                // Every cell of the next shell neighbours a skipped cell if this shell visited none, see ShellVisitMask
                // (not after the far faces with kNearFacesFirst: edges between two near faces have no parent there)
                if( mShell == D || ( !mShellVisit && ( mShell >= 2 || !kOrder.kNearFacesFirst ) ) )
                {
                    return nullptr;
                }
                mCell = kOrder.shellStart[++mShell];
                mVisit = mShellVisit = ShellVisitMask( distToBeat, ~mShellVisit );
            }

            while( !( mVisit & 1 ) )
            {
                mVisit >>= 1;
                mCell++;
            }
            mVisit >>= 1;
            return kOrder.nearest[mCell++];
        }

    private:
        FS_NEVERINLINE static float32v FS_VECTORCALL InitPruningNoInline( CellBounds<D>& bounds, std::array<int32v, 3>* cellPrimed, std::array<float32v, 3>* cellPos, bool relative, const float32v* pos,
                                                            bool nearFirst, DistanceFunction distFunc, bool ball, bool combineMax, float32v jitter, bool sizeJitterActive, const float32v& sizeJitter )
        {
            return InitPruning( bounds, cellPrimed, cellPos, relative, pos, nearFirst, distFunc, ball, combineMax, jitter, sizeJitterActive, sizeJitter );
        }

        // Reorders cell slots near side first if requested and fills the bounds, returns the ball bound radius
        // A feature point is within |jitter| of its cell centre, so per axis |delta| >= max( 0, |centre - pos| - |jitter| )
        FS_FORCEINLINE static float32v FS_VECTORCALL InitPruning( CellBounds<D>& bounds, std::array<int32v, 3>* cellPrimed, std::array<float32v, 3>* cellPos, bool relative, const float32v* pos,
                                                    bool nearFirst, DistanceFunction distFunc, bool ball, bool combineMax, float32v jitter, bool sizeJitterActive, const float32v& sizeJitter )
        {
            if( nearFirst )
            {
                NearFirst<D>( cellPrimed, cellPos, pos );
            }

            // Slack for approximate InvSqrt in the jitter offset and float rounding of positions/distances
            // Minkowski takes the 1 / P root with an approximate reciprocal of P, up to ~1e-3 relative error for P in [0.25, 8]
            float32v radius = FS::FMulAdd( FS::Abs( jitter ), float32v( 1 + 1.f / 512 ), float32v( 1.f / ( 1 << 20 ) ) );
            float32v scale( distFunc == DistanceFunction::Minkowski ? 1 - 1.f / 64 : 1 - 1.f / 4096 );
            float32v minFactor;

            if( sizeJitterActive )
            {
                minFactor = FS::Min( FS::FNMulAdd( sizeJitter, float32v( D == 2 ? 0x3ff : ( D == 3 ? 0xffff : 0xfffff ) ), float32v( 1 ) ), float32v( 1 ) );
                scale *= minFactor;
            }

            float32v ballRadius;
            if( ball )
            {
                float32v posSlack = FS::Abs( pos[0] );
                for( int d = 1; d < D; d++ )
                {
                    posSlack += FS::Abs( pos[d] );
                }
                ballRadius = FS::FMulAdd( posSlack, float32v( 1.f / ( 1 << 21 ) ), radius );

                if( sizeJitterActive )
                {
                    // Distances are scaled by at least minFactor: scale the squared centre distances by it, the radius by its square root
                    ballRadius *= FS::Sqrt( FS::Max( minFactor, float32v( 0 ) ) );
                }
            }

            for( int d = 0; d < D; d++ )
            {
                float32v axisRadius = FS::FMulAdd( FS::Abs( pos[d] ), float32v( 1.f / ( 1 << 21 ) ), radius );

                for( int i = 0; i < 3; i++ )
                {
                    // Cell centre - position
                    float32v centreOffset = relative ? cellPos[d][i] : cellPos[d][i] - pos[d];

                    if( ball )
                    {
                        bounds.axis[d][i] = sizeJitterActive ? centreOffset * centreOffset * minFactor : centreOffset * centreOffset;
                        continue;
                    }

                    float32v b = FS::Max( FS::Abs( centreOffset ) - axisRadius, float32v( 0 ) );

                    switch( distFunc )
                    {
                    default:
                        b *= b;
                        break;
                    case DistanceFunction::Manhattan:
                    case DistanceFunction::MaxAxis:
                    case DistanceFunction::Minkowski: // At least the largest axis distance for any P > 0
                        break;
                    case DistanceFunction::Hybrid:
                        b = FS::FMulAdd( b, b, b );
                        break;
                    }

                    bounds.axis[d][i] = b * scale;
                }
            }

            if( sizeJitterActive )
            {
                // Negative size factor allows negative distances, never skip those lanes
                mask32v valid = minFactor >= float32v( 0 );
                for( int d = 0; d < D; d++ )
                {
                    for( int i = 0; i < 3; i++ )
                    {
                        bounds.axis[d][i] = FS::Select( valid, bounds.axis[d][i], float32v( -kInfinity ) );
                    }
                }
            }

            if constexpr( D == 4 )
            {
                // Precombine axis pairs so each cell's bound is a single add/max
                for( int i = 0; i < 9; i++ )
                {
                    bounds.pairA[i] = Combine( combineMax, bounds.axis[0][i / 3], bounds.axis[1][i % 3] );
                    bounds.pairB[i] = Combine( combineMax, bounds.axis[2][i / 3], bounds.axis[3][i % 3] );
                }
            }
            return ballRadius;
        }

        FS_FORCEINLINE static float32v Combine( bool combineMax, float32v a, float32v b )
        {
            return combineMax ? FS::Max( a, b ) : a + b;
        }

        FS_FORCEINLINE float32v Bound( int cell ) const
        {
            if constexpr( D == 4 )
            {
                const int8_t* index = kOrder.pairIndex[cell];
                return Combine( mCombineMax, mBounds.pairA[index[0]], mBounds.pairB[index[1]] );
            }
            else if constexpr( D == 3 )
            {
                const int8_t* index = kOrder.nearest[cell];
                return Combine( mCombineMax, Combine( mCombineMax, mBounds.axis[0][index[0]], mBounds.axis[1][index[1]] ), mBounds.axis[2][index[2]] );
            }
            else
            {
                const int8_t* index = kOrder.nearest[cell];
                return Combine( mCombineMax, mBounds.axis[0][index[0]], mBounds.axis[1][index[1]] );
            }
        }

        // Bit per cell in the current shell, set if any lane might find a feature point closer than distToBeat in it.
        // Tested per shell rather than per cell so branches don't wait on the previous cell's distance.
        // Bounds only grow moving away from the centre cell, so cells next to an already skipped cell are skipped untested
        FS_FORCEINLINE uint32_t ShellVisitMask( float32v distToBeat, uint32_t skippedPrev ) const
        {
            if( mBall )
            {
                // Ball bounds hold squared centre distances (times the smallest size jitter factor): distance >= ( |centre - pos| - ballRadius )^2
                float32v t = FS::Sqrt( distToBeat ) + mBallRadius;
                distToBeat = t * t * float32v( 1 + 1.f / 1024 );
            }

            uint32_t visit = 0;
            for( int c = mCell; c < kOrder.shellStart[mShell + 1]; c++ )
            {
                if( !( kOrder.parentMask[c] & skippedPrev ) )
                {
                    visit |= (uint32_t)!AllLanes( Bound( c ) >= distToBeat ) << ( c - mCell );
                }
            }
            return visit;
        }

        static constexpr CellOrder<D> kOrder{};

        CellBounds<D>& mBounds;
        float32v mBallRadius;
        bool mPrune;
        bool mBall = false;
        bool mCombineMax = false;
        int mCell = 0;
        int mShell = 0; // Shell 0 is always visited
        uint32_t mVisit = kOrder.kFirstShellMask;
        uint32_t mShellVisit = kOrder.kFirstShellMask;
    };
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularValue, SIMD> final : public virtual CellularValue, public DispatchClass<Cellular<>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, true>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcf );
                yd = FS::FMulAdd( yd, invMag, ycf );

                int32v newCellValueHash = hash;
                float32v newDistance = CalcDistance<true>( distFunc, minkowskiP, xd, yd );
                if( sizeJitterActive )
                {
                    float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
                    newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                }

                for( int i = 0; ; i++ )
                {
                    mask32v closer = newDistance < distance[i];

                    float32v localDistance = distance[i];
                    int32v localCellValueHash = valueHash[i];

                    distance[i] = FS::Select( closer, newDistance, distance[i] );
                    valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                    if( i >= ( SPECIALISED ? COUNT - 1 : mValueIndex ) )
                    {
                        break;
                    }

                    newDistance = FS::Select( closer, localDistance, newDistance );
                    newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
                }

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->ScaleOutput( this, FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z );

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;
        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellOffset[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, x, cellPrimed[0], cellOffset[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, y, cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, z, cellPrimed[2], cellOffset[2] );

        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, valueIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, valueIndex ) ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
            xd = FS::FMulAdd( xd, invMag, cellOffset[0][o[0]] );
            yd = FS::FMulAdd( yd, invMag, cellOffset[1][o[1]] );
            zd = FS::FMulAdd( zd, invMag, cellOffset[2][o[2]] );

            int32v newCellValueHash = hash;
            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            for( int i = 0; ; i++ )
            {
                mask32v closer = newDistance < distance[i];

                float32v localDistance = distance[i];
                int32v localCellValueHash = valueHash[i];

                distance[i] = FS::Select( closer, newDistance, distance[i] );
                valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                if( i >= valueIndex )
                {
                    break;
                }

                newDistance = FS::Select( closer, localDistance, newDistance );
                newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
            }
        }

        return this->ScaleOutput( this, FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z, w );

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;
        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellOffset[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, x, cellPrimed[0], cellOffset[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, y, cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, z, cellPrimed[2], cellOffset[2] );
        this->CellAxis( FS::Convert<int32_t>( w ) + int32v( -1 ), Primes::W, w, cellPrimed[3], cellOffset[3] );

        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, valueIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, valueIndex ) ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 8 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v zd = FS::Convert<float>( ( hash >> 16 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
            xd = FS::FMulAdd( xd, invMag, cellOffset[0][o[0]] );
            yd = FS::FMulAdd( yd, invMag, cellOffset[1][o[1]] );
            zd = FS::FMulAdd( zd, invMag, cellOffset[2][o[2]] );
            wd = FS::FMulAdd( wd, invMag, cellOffset[3][o[3]] );

            int32v newCellValueHash = hash;
            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            for( int i = 0; ; i++ )
            {
                mask32v closer = newDistance < distance[i];

                float32v localDistance = distance[i];
                int32v localCellValueHash = valueHash[i];

                distance[i] = FS::Select( closer, newDistance, distance[i] );
                valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                if( i >= valueIndex )
                {
                    break;
                }

                newDistance = FS::Select( closer, localDistance, newDistance );
                newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
            }
        }

        return this->ScaleOutput( this, FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularDistance, SIMD> final : public virtual CellularDistance, public DispatchClass<Cellular<>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    // Number of closest distances to track: up to the largest distance index the output depends on
    int GetDistanceCount() const
    {
        return std::max( mDistanceIndex0, mReturnType == ReturnType::Index0 ? 0 : mDistanceIndex1 ) + 1;
    }

    // One 2D cell, shared by the plain 3x3 loop and the pruned loop (a lambda isn't reliably inlined)
    template<int COUNT>
    FS_FORCEINLINE void VisitCell2D( std::array<float32v, kMaxDistanceCount>& distance, DistanceFunction distFunc, float32v minkowskiP, float32v jitter, bool sizeJitterActive,
                                     const float32v& sizeJitter, int32v seed, int32v xPrimed, int32v yPrimed, float32v xOffset, float32v yOffset ) const
    {
        int32v hash = HashPrimesHB( seed, xPrimed, yPrimed );
        float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
        float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
        xd = FS::FMulAdd( xd, invMag, xOffset );
        yd = FS::FMulAdd( yd, invMag, yOffset );

        float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd );
        if( sizeJitterActive )
        {
            float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
            newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
        }

        for( int i = COUNT - 1; i > 0; i-- )
        {
            distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
        }

        distance[0] = FS::Min( distance[0], newDistance );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y );

        if constexpr( SPECIALISED )
        {
            int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
            int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

            float32v xcf = FS::Convert<float>( xc );
            float32v ycfBase = FS::Convert<float>( ycBase );

            xc *= int32v( Primes::X );
            ycBase *= int32v( Primes::Y );

            for( int xi = 0; xi < 3; xi++ )
            {
                float32v xcfOffset = xcf - x;
                float32v ycf = ycfBase;
                int32v yc = ycBase;
                for ( int yi = 0; yi < 3; yi++ )
                {
                    VisitCell2D<COUNT>( distance, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, xc, yc, xcfOffset, ycf - y );

                    ycf += float32v( 1 );
                    yc += int32v( Primes::Y );
                }
                xcf += float32v( 1 );
                xc += int32v( Primes::X );
            }
        }
        else // 2D cells are cheap: only the generic variant's are expensive enough for pruning to pay off
        {
            float32v pos[] = { x, y };

            std::array<int32v, 3> cellPrimed[2];
            std::array<float32v, 3> cellOffset[2];
            this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellOffset[0] );
            this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellOffset[1] );
            for( int d = 0; d < 2; d++ )
            {
                for( int i = 0; i < 3; i++ )
                {
                    cellOffset[d][i] -= pos[d];
                }
            }

            int pruneIndex = GetDistanceCount() - 1;
            typename DispatchClass::template CellBounds<2> bounds;
            typename DispatchClass::template CellIterator<2> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

            while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
            {
                VisitCell2D<COUNT>( distance, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellOffset[0][o[0]], cellOffset[1][o[1]] );
            }
        }

        return GetReturn( distance, 1 + this->kJitter2D );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z );

        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellOffset[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellOffset[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellOffset[2] );
        for( int d = 0; d < 3; d++ )
        {
            for( int i = 0; i < 3; i++ )
            {
                cellOffset[d][i] -= pos[d];
            }
        }

        int pruneIndex = SPECIALISED ? COUNT - 1 : GetDistanceCount() - 1;
        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
            xd = FS::FMulAdd( xd, invMag, cellOffset[0][o[0]] );
            yd = FS::FMulAdd( yd, invMag, cellOffset[1][o[1]] );
            zd = FS::FMulAdd( zd, invMag, cellOffset[2][o[2]] );

            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            for( int i = COUNT - 1; i > 0; i-- )
            {
                distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
            }

            distance[0] = FS::Min( distance[0], newDistance );
        }

        return GetReturn( distance, 1 + this->kJitter3D );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z, w );

        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellOffset[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellOffset[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellOffset[2] );
        this->CellAxis( FS::Convert<int32_t>( w ) + int32v( -1 ), Primes::W, float32v( 0 ), cellPrimed[3], cellOffset[3] );
        for( int d = 0; d < 4; d++ )
        {
            for( int i = 0; i < 3; i++ )
            {
                cellOffset[d][i] -= pos[d];
            }
        }

        int pruneIndex = SPECIALISED ? COUNT - 1 : GetDistanceCount() - 1;
        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 8 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v zd = FS::Convert<float>( ( hash >> 16 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
            xd = FS::FMulAdd( xd, invMag, cellOffset[0][o[0]] );
            yd = FS::FMulAdd( yd, invMag, cellOffset[1][o[1]] );
            zd = FS::FMulAdd( zd, invMag, cellOffset[2][o[2]] );
            wd = FS::FMulAdd( wd, invMag, cellOffset[3][o[3]] );

            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            for( int i = COUNT - 1; i > 0; i-- )
            {
                distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
            }

            distance[0] = FS::Min( distance[0], newDistance );
        }

        return GetReturn( distance, 1 + this->kJitter4D );
    }

    FS_FORCEINLINE float32v GetReturn( std::array<float32v, kMaxDistanceCount>& distance, float maxDist ) const
    {
        if( mDistanceFunction == FastNoise::DistanceFunction::Euclidean )
        {
            distance[mDistanceIndex0] = FastLengthSqrt( distance[mDistanceIndex0] );

            // Avoid double sqrt when both indices are the same
            if( mDistanceIndex1 != mDistanceIndex0 )
            {
                distance[mDistanceIndex1] = FastLengthSqrt( distance[mDistanceIndex1] );
            }
        }

        maxDist *= maxDist;

        switch( mReturnType )
        {
        default:
        case ReturnType::Index0:
        {
            return this->ScaleOutput( this, distance[mDistanceIndex0], 0, maxDist );
        }
        case ReturnType::Index0Add1:
        {
            return this->ScaleOutput( this, distance[mDistanceIndex0] + distance[mDistanceIndex1], 0, maxDist * 2 );
        }
        case ReturnType::Index0Sub1:
        {
            return this->ScaleOutput( this, FS::Abs( distance[mDistanceIndex0] - distance[mDistanceIndex1] ), 0, maxDist );
        }
        case ReturnType::Index0Mul1:
        {
            return this->ScaleOutput( this, distance[mDistanceIndex0] * distance[mDistanceIndex1], 0, maxDist * maxDist );
        }
        case ReturnType::Index0Div1:
        {
            return this->ScaleOutput( this, distance[mDistanceIndex0] * FS::Reciprocal( distance[mDistanceIndex1] ), 0, maxDist );
        }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularLookup, SIMD> final : public virtual CellularLookup, public DispatchClass<Cellular<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    // One 2D cell, shared by the plain 3x3 loop and the pruned loop (a lambda isn't reliably inlined)
    FS_FORCEINLINE void VisitCell2D( float32v& distance, float32v& cellX, float32v& cellY, DistanceFunction distFunc, float32v minkowskiP, float32v jitter, bool sizeJitterActive,
                                     const float32v& sizeJitter, int32v seed, float32v x, float32v y, int32v xPrimed, int32v yPrimed, float32v xCell, float32v yCell ) const
    {
        int32v hash = HashPrimesHB( seed, xPrimed, yPrimed );
        float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
        float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
        float32v localCellX = FS::FMulAdd( xd, invMag, xCell );
        float32v localCellY = FS::FMulAdd( yd, invMag, yCell );
        xd = localCellX - x;
        yd = localCellY - y;

        float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd );
        if( sizeJitterActive )
        {
            float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
            newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
        }

        mask32v closer = newDistance < distance;
        distance = FS::Min( newDistance, distance );

        cellX = FS::Select( closer, localCellX, cellX );
        cellY = FS::Select( closer, localCellY, cellY );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY;

        this->ScalePositions( this, x, y );

        if constexpr( SPECIALISED )
        {
            int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
            int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

            float32v xcf = FS::Convert<float>( xc );
            float32v ycfBase = FS::Convert<float>( ycBase );

            xc *= int32v( Primes::X );
            ycBase *= int32v( Primes::Y );

            for( int xi = 0; xi < 3; xi++ )
            {
                float32v ycf = ycfBase;
                int32v yc = ycBase;
                for( int yi = 0; yi < 3; yi++ )
                {
                    VisitCell2D( distance, cellX, cellY, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, x, y, xc, yc, xcf, ycf );

                    ycf += float32v( 1 );
                    yc += int32v( Primes::Y );
                }
                xcf += float32v( 1 );
                xc += int32v( Primes::X );
            }
        }
        else // 2D cells are cheap: only the generic variant's are expensive enough for pruning to pay off
        {
            float32v pos[] = { x, y };

            std::array<int32v, 3> cellPrimed[2];
            std::array<float32v, 3> cellPos[2];
            this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0] );
            this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );

            typename DispatchClass::template CellBounds<2> bounds;
            typename DispatchClass::template CellIterator<2> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

            while( const int8_t* o = cells.Next( distance ) )
            {
                VisitCell2D( distance, cellX, cellY, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, x, y, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPos[0][o[0]], cellPos[1][o[1]] );
            }
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ;

        this->ScalePositions( this, x, y, z );

        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellPos[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellPos[2] );

        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( distance ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
            float32v localCellX = FS::FMulAdd( xd, invMag, cellPos[0][o[0]] );
            float32v localCellY = FS::FMulAdd( yd, invMag, cellPos[1][o[1]] );
            float32v localCellZ = FS::FMulAdd( zd, invMag, cellPos[2][o[2]] );
            xd = localCellX - x;
            yd = localCellY - y;
            zd = localCellZ - z;

            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            mask32v closer = newDistance < distance;
            distance = FS::Min( newDistance, distance );

            cellX = FS::Select( closer, localCellX, cellX );
            cellY = FS::Select( closer, localCellY, cellY );
            cellZ = FS::Select( closer, localCellZ, cellZ );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ), cellZ * float32v( mScale ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    FN_ALIGN_STACK float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = SPECIALISED ? SIZE_JITTER : ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ, cellW;

        this->ScalePositions( this, x, y, z, w );

        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellPos[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellPos[2] );
        this->CellAxis( FS::Convert<int32_t>( w ) + int32v( -1 ), Primes::W, float32v( 0 ), cellPrimed[3], cellPos[3] );

        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( distance ) )
        {
            int32v hash = HashPrimesHB( cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
            float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v yd = FS::Convert<float>( ( hash >> 8 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v zd = FS::Convert<float>( ( hash >> 16 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

            float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
            float32v localCellX = FS::FMulAdd( xd, invMag, cellPos[0][o[0]] );
            float32v localCellY = FS::FMulAdd( yd, invMag, cellPos[1][o[1]] );
            float32v localCellZ = FS::FMulAdd( zd, invMag, cellPos[2][o[2]] );
            float32v localCellW = FS::FMulAdd( wd, invMag, cellPos[3][o[3]] );
            xd = localCellX - x;
            yd = localCellY - y;
            zd = localCellZ - z;
            wd = localCellW - w;

            float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
            if( sizeJitterActive )
            {
                float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
            }

            mask32v closer = newDistance < distance;
            distance = FS::Min( newDistance, distance );

            cellX = FS::Select( closer, localCellX, cellX );
            cellY = FS::Select( closer, localCellY, cellY );
            cellZ = FS::Select( closer, localCellZ, cellZ );
            cellW = FS::Select( closer, localCellW, cellW );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ), cellZ * float32v( mScale ), cellW * float32v( mScale ) );
    }
};
