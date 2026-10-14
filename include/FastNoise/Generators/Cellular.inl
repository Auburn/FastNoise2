#include <cfloat>
#include <array>
#include <type_traits>
#include <utility>

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
    // Euclidean finds the same closest cells as EuclideanSquared, so it shares those variants
    template<int MAX_COUNT, typename FUNC>
    FS_FORCEINLINE auto SelectGenT( int count, FUNC&& func ) const
    {
        bool sizeJitterActive = this->mSizeJitter.simdGeneratorPtr || this->mSizeJitter.constant != 0.0f;

        switch( this->mDistanceFunction )
        {
        case DistanceFunction::Euclidean:
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
        case DistanceFunction::Hybrid:
        case DistanceFunction::MaxAxis:
            // Only the common closest cell case is specialised, to limit code size
            if( !sizeJitterActive && count == 1 )
            {
                return this->mDistanceFunction == DistanceFunction::Hybrid ?
                    func( std::true_type{}, DistanceFunctionConstant<DistanceFunction::Hybrid>{}, CountConstant<1>{}, std::false_type{} ) :
                    func( std::true_type{}, DistanceFunctionConstant<DistanceFunction::MaxAxis>{}, CountConstant<1>{}, std::false_type{} );
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

    // All lanes within 1 of lane 0 on every axis
    template<int D>
    FS_FORCEINLINE static bool IsCoherent( const float32v* pos )
    {
        mask32v coherent = FS::Abs( pos[0] - float32v( FS::Extract0( pos[0] ) ) ) <= float32v( 1 );
        for( int d = 1; d < D; d++ )
        {
            coherent &= FS::Abs( pos[d] - float32v( FS::Extract0( pos[d] ) ) ) <= float32v( 1 );
        }
        return AllLanes( coherent );
    }

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
        if( IsCoherent<D>( pos ) )
        {
            return CellOrder<D>::kNearFacesFirst ? kPruneNearFirst : kPruneCoherent;
        }
        if( D == 4 || ( distanceIndex <= 2 && AllLanes( FS::Abs( jitter ) <= float32v( maxJitter * 1.001f ) ) ) )
        {
            return kPruneNearFirst;
        }
        return kPruneNone;
    }

    // Constant indices only, so distance/hash arrays can stay in registers
    template<typename T, size_t N>
    FS_FORCEINLINE static T DistanceAt( const std::array<T, N>& distance, int index )
    {
        T value = distance[0];
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
        // NEON has no movemask, BitMask() costs a horizontal add per native half: test that no lane is clear instead
        if constexpr( SIMD & FastSIMD::FeatureFlag::ARM )
        {
            return !FS::AnyMask( ~m );
        }
        else
        {
            return FS::BitMask( m ) == (FS::BitStorage<mask32v::ElementCount>)( ~0ull >> ( 64 - mask32v::ElementCount ) );
        }
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

    // The 2^D cells around a position (floor and floor + 1 per axis) always hold the closest point when the jitter radius is at most:
    // Euclidean: ( sqrt( 1 + ( D - 1 ) / 4 ) - sqrt( ( D - 1 ) / 4 ) ) / 2, worst case at a cell edge midpoint
    // Manhattan: 1 / ( 2 * sqrt( D ) ), larger in 2D/3D so Hybrid (EuclideanSquared + Manhattan) can use the Euclidean bound
    // MaxAxis: 0.25. Returns 0 for distance functions without a bound
    template<int D>
    static constexpr float FloorCellsJitterBound( DistanceFunction distFunc )
    {
        switch( distFunc )
        {
        case DistanceFunction::Euclidean:
        case DistanceFunction::EuclideanSquared:
        case DistanceFunction::Manhattan:
        case DistanceFunction::Hybrid:
            return D == 2 ? 0.309017f : ( D == 3 ? 0.258819f : 0.228425f );
        case DistanceFunction::MaxAxis:
            return 0.25f;
        default:
            return 0;
        }
    }

    // Only the closest point and a constant grid jitter within the bound (with slack for approximate InvSqrt in the jitter offset)
    // Not used in 4D: with spatially coherent lanes the pruned 3^D search visits fewer cells
    // Not used without jitter: every point is on the lattice, exact distance ties must keep the 3^D search's cell order
    template<int D>
    FS_FORCEINLINE bool FloorCellsActive( int count, float jitterScale ) const
    {
        // Largest bound first: rejects the default grid jitter without the distance function switch
        float scaledJitter = std::abs( this->mGridJitter.constant * jitterScale );
        if( D == 4 || !( scaledJitter <= FloorCellsJitterBound<D>( DistanceFunction::Euclidean ) * ( 1 - 1.f / 1024 ) ) )
        {
            return false;
        }

        bool sizeJitterActive = this->mSizeJitter.simdGeneratorPtr || this->mSizeJitter.constant != 0.0f;

        if( count > 1 || sizeJitterActive || this->mGridJitter.simdGeneratorPtr || this->mGridJitter.constant == 0.0f )
        {
            return false;
        }
        return scaledJitter <= FloorCellsJitterBound<D>( this->mDistanceFunction ) * ( 1 - 1.f / 1024 );
    }

    // Calls func( specialised, distanceFunction ) with compile time constants for common distance functions
    template<typename FUNC>
    FS_FORCEINLINE auto SelectFloorGenT( FUNC&& func ) const
    {
        switch( this->mDistanceFunction )
        {
        case DistanceFunction::Euclidean:
        case DistanceFunction::EuclideanSquared:
            return func( std::true_type{}, DistanceFunctionConstant<DistanceFunction::EuclideanSquared>{} );
        case DistanceFunction::Manhattan:
            return func( std::true_type{}, DistanceFunctionConstant<DistanceFunction::Manhattan>{} );
        default:
            return func( std::false_type{}, DistanceFunctionConstant<DistanceFunction::EuclideanSquared>{} );
        }
    }

    // Jittered feature point offset from its cell centre (on a sphere of radius |jitter|) + base
    template<int D>
    FS_FORCEINLINE static void FeaturePointOffset( int32v hash, float32v jitter, const float32v* base, float32v* offset )
    {
        if constexpr( D == 2 )
        {
            offset[0] = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
            offset[1] = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );
        }
        else if constexpr( D == 3 )
        {
            offset[0] = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            offset[1] = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
            offset[2] = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );
        }
        else
        {
            offset[0] = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            offset[1] = FS::Convert<float>( ( hash >> 8 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            offset[2] = FS::Convert<float>( ( hash >> 16 ) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
            offset[3] = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );
        }

        float32v magSqr = offset[0] * offset[0];
        for( int d = 1; d < D; d++ )
        {
            magSqr = FS::FMulAdd( offset[d], offset[d], magSqr );
        }
        float32v invMag = jitter * FS::InvSqrt( magSqr );
        for( int d = 0; d < D; d++ )
        {
            offset[d] = FS::FMulAdd( offset[d], invMag, base[d] );
        }
    }

    template<size_t... I>
    FS_FORCEINLINE static float32v CalcDistanceArray( DistanceFunction distFunc, const float32v* delta, std::index_sequence<I...> )
    {
        return CalcDistance<false>( distFunc, float32v( 0 ), delta[I]... );
    }

    // Feature point of the cell with hash input ( seed ^ primed... ), keeps it if closer. base is the cell's offset from the position,
    // or with CELL its absolute position: the distance is then measured like CellularLookup's 3^D search, so both pick the same cell
    template<int D, bool HASH, bool CELL>
    FS_FORCEINLINE static void VisitFloorCell( DistanceFunction distFunc, int32v hash, float32v jitter, const float32v* base, const float32v* pos, float32v& distance, int32v* closestHash, float32v* closestCell )
    {
        hash *= int32v( HashMultiplier::A );

        float32v point[D], delta[D];
        FeaturePointOffset<D>( hash, jitter, base, point );
        for( int d = 0; d < D; d++ )
        {
            delta[d] = CELL ? point[d] - pos[d] : point[d];
        }
        float32v newDistance = CalcDistanceArray( distFunc, delta, std::make_index_sequence<D>{} );

        mask32v closer = newDistance < distance;
        distance = FS::Min( newDistance, distance );

        if constexpr( HASH )
        {
            *closestHash = FS::Select( closer, hash, *closestHash );
        }
        if constexpr( CELL )
        {
            for( int d = 0; d < D; d++ )
            {
                closestCell[d] = FS::Select( closer, point[d], closestCell[d] );
            }
        }
    }

    // Start of a closest point search: nothing found yet
    template<int D, bool HASH, bool CELL>
    FS_FORCEINLINE static float32v FloorCellsInit( int32v* closestHash, float32v* closestCell )
    {
        if constexpr( HASH )
        {
            *closestHash = int32v( 0 );
        }
        if constexpr( CELL )
        {
            for( int d = 0; d < D; d++ )
            {
                closestCell[d] = float32v( 0 );
            }
        }
        return float32v( kInfinity );
    }

    // Closest feature point among the 2^D cells around the position, see FloorCellsJitterBound(). Returns its distance (no sqrt),
    // optionally its cell hash and its absolute position (see VisitFloorCell()). Cells in the 3^D loops' order (axis 0 slowest)
    template<int D, bool HASH, bool CELL>
    FS_FORCEINLINE static float32v FloorCellsClosest( DistanceFunction distFunc, int32v seed, float32v jitter, const float32v* pos, int32v* closestHash, float32v* closestCell )
    {
        int32v primed[D][2];
        float32v cellOffset[D][2];

        for( int d = 0; d < D; d++ )
        {
            float32v floored = FS::Floor( pos[d] );
            primed[d][0] = FloorToInt( pos[d], floored ) * int32v( Primes::Lookup[d] );
            primed[d][1] = primed[d][0] + int32v( Primes::Lookup[d] );
            cellOffset[d][0] = CELL ? floored : floored - pos[d];
            cellOffset[d][1] = cellOffset[d][0] + float32v( 1 );
        }

        float32v distance = FloorCellsInit<D, HASH, CELL>( closestHash, closestCell );

#if defined( _MSC_VER ) && !defined( __clang__ )
        // MSVC miscompiled the computed cell indexing below for AVX512, explicit loops instead (~5% slower with clang)
        for( int xi = 0; xi < 2; xi++ )
        {
            int32v seedX = seed ^ primed[0][xi];
            for( int yi = 0; yi < 2; yi++ )
            {
                int32v seedXY = seedX ^ primed[1][yi];
                for( int zi = 0; zi < ( D == 3 ? 2 : 1 ); zi++ )
                {
                    float32v base[D];
                    base[0] = cellOffset[0][xi];
                    base[1] = cellOffset[1][yi];
                    if constexpr( D == 3 )
                    {
                        base[2] = cellOffset[2][zi];
                        VisitFloorCell<D, HASH, CELL>( distFunc, seedXY ^ primed[2][zi], jitter, base, pos, distance, closestHash, closestCell );
                    }
                    else
                    {
                        VisitFloorCell<D, HASH, CELL>( distFunc, seedXY, jitter, base, pos, distance, closestHash, closestCell );
                    }
                }
            }
        }
#else
        // The last axis varies fastest so the partial hashes of the lower axes are shared
        for( int c = 0; c < ( 1 << D ); c++ )
        {
            int32v hash = seed;
            float32v base[D];
            for( int d = 0; d < D; d++ )
            {
                hash ^= primed[d][( c >> ( D - 1 - d ) ) & 1];
                base[d] = cellOffset[d][( c >> ( D - 1 - d ) ) & 1];
            }
            VisitFloorCell<D, HASH, CELL>( distFunc, hash, jitter, base, pos, distance, closestHash, closestCell );
        }
#endif
        return distance;
    }

    // Closest2D() pays off when the jitter is small enough that the 2x2 cells usually hold the closest point and the lanes are close together.
    // Incoherent lanes are each an independent chance of needing the cells beyond (~6% per lane), still a win with SSE4.1's 8 lanes per vector
    // (not SSE2: no native floor)
    // Coherence first, one axis at a time: most incoherent batches already fail on x. A constant grid jitter is tested as a scalar
    FS_FORCEINLINE bool Closest2DActive( float32v x, float32v y, float32v jitter ) const
    {
        constexpr bool kIncoherent = float32v::ElementCount <= 8 && ( SIMD & FastSIMD::FeatureFlag::SSE41 );
        constexpr float kMaxJitter = kJitter2D * 1.001f;
        if constexpr( !kIncoherent )
        {
            if( !IsCoherent<1>( &x ) || !IsCoherent<1>( &y ) )
            {
                return false;
            }
        }
        return this->mGridJitter.simdGeneratorPtr ? AllLanes( FS::Abs( jitter ) <= float32v( kMaxJitter ) ) : std::abs( kJitter2D * this->mGridJitter.constant ) <= kMaxJitter;
    }

    // 3D: worth checking the 2^D cells first: coherent lanes rarely need more, incoherent lanes are each an independent chance of needing the
    // 3^D search too, worth it up to 16 lanes and not with Manhattan's looser bound
    // Not without jitter: every point is on the lattice, exact distance ties must keep the 3^D search's cell order
    // CellularLookup only with coherent lanes (INCOHERENT = false): a seeded search picked another cell on rare lanes
    template<bool INCOHERENT = true>
    FS_FORCEINLINE static bool FloorFirst3DActive( DistanceFunction distFunc, float32v jitter, const float32v* pos )
    {
        if( !AllLanes( ( FS::Abs( jitter ) <= float32v( kJitter3D * 1.001f ) ) & ( jitter != float32v( 0 ) ) ) )
        {
            return false;
        }
        return ( INCOHERENT && float32v::ElementCount <= 16 && distFunc != DistanceFunction::Manhattan ) || IsCoherent<3>( pos );
    }

    // True if no lane can have a closer point than distance in the other cells of the 3^D search, after the 2^D cells around the position
    // (FloorCellsClosest()): the closest centre of those cells is ( 1 + edge, edge, ... ) away with the 1 on the axis with the smallest edge
    // distance, and their points are within the jitter radius of the centre. EuclideanSquared/Manhattan distance only
    template<int D>
    FS_FORCEINLINE static bool FloorCellsComplete( DistanceFunction distFunc, float32v distance, float32v jitter, const float32v* pos )
    {
        float32v edge[D];
        for( int d = 0; d < D; d++ )
        {
            float32v t = pos[d] - FS::Floor( pos[d] );
            edge[d] = FS::Min( t, float32v( 1 ) - t );
        }

        float32v radius = FS::FMulAdd( FS::Abs( jitter ), float32v( 1 + 1.f / 512 ), float32v( 1.f / ( 1 << 20 ) ) );
        mask32v closer;
        if( distFunc == DistanceFunction::Manhattan )
        {
            float32v centreDist = float32v( 1 ) + edge[0];
            for( int d = 1; d < D; d++ )
            {
                centreDist += edge[d];
            }
            closer = FS::FMulAdd( radius, float32v( D == 2 ? kRoot2f : ( D == 3 ? kRoot3f : 2.0f ) ), distance ) > centreDist * float32v( 1 - 1.f / 4096 );
        }
        else
        {
            float32v edgeSqr = edge[0] * edge[0];
            float32v minEdge = edge[0];
            for( int d = 1; d < D; d++ )
            {
                edgeSqr = FS::FMulAdd( edge[d], edge[d], edgeSqr );
                minEdge = FS::Min( minEdge, edge[d] );
            }
            float32v reach = FS::Sqrt( distance ) + radius;
            closer = reach * reach > FS::FMulAdd( minEdge, float32v( 2 ), edgeSqr + float32v( 1 ) ) * float32v( 1 - 1.f / 4096 );
        }
        return !FS::AnyMask( closer );
    }

    // 2D closest point for EuclideanSquared/Manhattan, same result as the 3x3 cell search when the jitter radius is at most kJitter2D
    // (see Closest2DActive()): the 2x2 cells around the position first, the 4 cells beyond them only if a lane could have a closer point
    // there (~6% of 16 lane batches on a uniform grid at grid jitter 1). The 3x3's far corner never can.
    // Cells beyond are on the far side of the 2x2 pair: below the pair in the lower half of a cell
    template<bool HASH, bool CELL>
    FS_FORCEINLINE static float32v Closest2D( DistanceFunction distFunc, int32v seed, float32v jitter, float32v x, float32v y, int32v* closestHash, float32v* closestCell )
    {
        float32v pos[] = { x, y };
        int32v cell[2], primed[2][2];
        float32v floored[2], t[2], edge[2];

        for( int d = 0; d < 2; d++ )
        {
            floored[d] = FS::Floor( pos[d] );
            t[d] = pos[d] - floored[d];
            cell[d] = FloorToInt( pos[d], floored[d] );
            primed[d][0] = cell[d] * int32v( Primes::Lookup[d] );
            primed[d][1] = primed[d][0] + int32v( Primes::Lookup[d] );
            edge[d] = FS::Min( t[d], float32v( 1 ) - t[d] );
        }

        float32v distance = FloorCellsInit<2, HASH, CELL>( closestHash, closestCell );
        for( int xi = 0; xi < 2; xi++ )
        {
            int32v seedX = seed ^ primed[0][xi];
            for( int yi = 0; yi < 2; yi++ )
            {
                float32v base[2];
                base[0] = CELL ? floored[0] + float32v( (float)xi ) : float32v( (float)xi ) - t[0];
                base[1] = CELL ? floored[1] + float32v( (float)yi ) : float32v( (float)yi ) - t[1];
                VisitFloorCell<2, HASH, CELL>( distFunc, seedX ^ primed[1][yi], jitter, base, pos, distance, closestHash, closestCell );
            }
        }

        // A cell beyond can only be closer if its centre is within distance + radius, its point is on the jitter radius around the centre
        float32v radius = FS::FMulAdd( FS::Abs( jitter ), float32v( 1 + 1.f / 512 ), float32v( 1.f / ( 1 << 20 ) ) );
        float32v reach;
        if( distFunc == DistanceFunction::Manhattan )
        {
            reach = FS::FMulAdd( radius, float32v( kRoot2f ), distance );
        }
        else
        {
            reach = FS::Sqrt( distance ) + radius;
            reach *= reach;
        }

        // Cells beyond the 2x2: their centres are at least ( 1 + edge[0], edge[1] ) or ( edge[0], 1 + edge[1] ) away
        mask32v visit;
        if( distFunc == DistanceFunction::Manhattan )
        {
            visit = reach > ( float32v( 1 ) + edge[0] + edge[1] ) * float32v( 1 - 1.f / 4096 );
        }
        else
        {
            float32v centreDistSqr = FS::Min( FS::FMulAdd( edge[0] + float32v( 1 ), edge[0] + float32v( 1 ), edge[1] * edge[1] ),
                                              FS::FMulAdd( edge[1] + float32v( 1 ), edge[1] + float32v( 1 ), edge[0] * edge[0] ) );
            visit = reach > centreDistSqr * float32v( 1 - 1.f / 4096 );
        }

        if( FS::AnyMask( visit ) )
        {
            for( int d = 0; d < 2; d++ )
            {
                // Far side cell on axis d, matches the 3x3 search's round half to even at t == 0.5
                mask32v lowerHalf = FS::Convert<int32_t>( pos[d] ) == cell[d];
                int32v farPrimed = primed[d][0] + FS::Select( lowerHalf, int32v( (int32_t)( 0u - (uint32_t)Primes::Lookup[d] ) ), int32v( (int32_t)( (uint32_t)Primes::Lookup[d] * 2u ) ) );
                float32v farOffset = CELL ? floored[d] + FS::Select( lowerHalf, float32v( -1 ), float32v( 2 ) ) : FS::Select( lowerHalf, float32v( -1 ) - t[d], float32v( 2 ) - t[d] );

                for( int i = 0; i < 2; i++ )
                {
                    float32v otherOffset = CELL ? floored[d ^ 1] + float32v( (float)i ) : float32v( (float)i ) - t[d ^ 1];
                    float32v base[] = { d ? otherOffset : farOffset, d ? farOffset : otherOffset };
                    VisitFloorCell<2, HASH, CELL>( distFunc, seed ^ farPrimed ^ primed[d ^ 1][i], jitter, base, pos, distance, closestHash, closestCell );
                }
            }
        }
        return distance;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularValue, SIMD> final : public virtual CellularValue, public DispatchClass<Cellular<>, SIMD>
{
    // Names the cellular base for its member templates (CellBounds, CellIterator); GCC 16 rejects reaching them through the injected class name
    using CellularBase = DispatchClass<Cellular<>, SIMD>;

    FASTNOISE_IMPL_GEN_FN( Gen );

    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter2D * mGridJitter.constant );

        this->ScalePositions( this, x, y );

        float32v pos[] = { x, y };
        int32v hash;
        this->template FloorCellsClosest<2, true, false>( distFunc, seed, jitter, pos, &hash, nullptr );

        return this->ScaleOutput( this, FS::Convert<float>( hash ), -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        if( this->template FloorCellsActive<2>( mValueIndex + 1, this->kJitter2D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y );
        }

        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
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

        std::array<int32v, COUNT> valueHash;
        std::array<float32v, COUNT> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y );

        if constexpr( SPECIALISED && COUNT == 1 && !SIZE_JITTER && ( DIST_FUNC == DistanceFunction::EuclideanSquared || DIST_FUNC == DistanceFunction::Manhattan ) )
        {
            if( this->Closest2DActive( x, y, jitter ) )
            {
                int32v hash;
                this->template Closest2D<true, false>( distFunc, seed, jitter, x, y, &hash, nullptr );
                return this->ScaleOutput( this, FS::Convert<float>( hash ), -kValueBounds, kValueBounds );
            }
        }

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;

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
                float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd );
                if( sizeJitterActive )
                {
                    float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
                    newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                }

                for( int i = 0; i < COUNT; i++ )
                {
                    mask32v closer = newDistance < distance[i];

                    int32v localCellValueHash = valueHash[i];
                    valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                    if( i >= valueIndex )
                    {
                        distance[i] = FS::Select( closer, newDistance, distance[i] );
                        break;
                    }

                    // Same as selecting on closer (incl. ties), but SSE2 doesn't match these selects to min/max itself
                    float32v localDistance = distance[i];
                    distance[i] = FS::Min( newDistance, distance[i] );
                    newDistance = FS::Max( localDistance, newDistance );
                    newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
                }

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->ScaleOutput( this, FS::Convert<float>( this->DistanceAt( valueHash, valueIndex ) ), -kValueBounds, kValueBounds );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter3D * mGridJitter.constant );

        this->ScalePositions( this, x, y, z );

        float32v pos[] = { x, y, z };
        int32v hash;
        this->template FloorCellsClosest<3, true, false>( distFunc, seed, jitter, pos, &hash, nullptr );

        return this->ScaleOutput( this, FS::Convert<float>( hash ), -kValueBounds, kValueBounds );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        if( this->template FloorCellsActive<3>( mValueIndex + 1, this->kJitter3D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y, z );
        }

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

        std::array<int32v, COUNT> valueHash;
        std::array<float32v, COUNT> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z );

        // The 2^D cells around the position usually hold the closest point, the 3^D search below only runs if a lane might be missing it
        if constexpr( SPECIALISED && COUNT == 1 && !SIZE_JITTER && ( DIST_FUNC == DistanceFunction::EuclideanSquared || DIST_FUNC == DistanceFunction::Manhattan ) )
        {
            float32v floorPos[] = { x, y, z };
            if( this->FloorFirst3DActive( distFunc, jitter, floorPos ) )
            {
                int32v hash;
                float32v closest = this->template FloorCellsClosest<3, true, false>( distFunc, seed, jitter, floorPos, &hash, nullptr );
                if( this->template FloorCellsComplete<3>( distFunc, closest, jitter, floorPos ) )
                {
                    return this->ScaleOutput( this, FS::Convert<float>( hash ), -kValueBounds, kValueBounds );
                }
                // The 3^D search starts from the closest point so far, its pruning then skips most cells
                distance[0] = closest;
                valueHash[0] = hash;
            }
        }

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;
        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellOffset[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, x, cellPrimed[0], cellOffset[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, y, cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, z, cellPrimed[2], cellOffset[2] );

        typename CellularBase::template CellBounds<3> bounds;
        typename CellularBase::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, valueIndex, jitter, sizeJitterActive, sizeJitter );

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

            for( int i = 0; i < COUNT; i++ )
            {
                mask32v closer = newDistance < distance[i];

                int32v localCellValueHash = valueHash[i];
                valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                if( i >= valueIndex )
                {
                    distance[i] = FS::Select( closer, newDistance, distance[i] );
                    break;
                }

                // Same as selecting on closer (incl. ties), but SSE2 doesn't match these selects to min/max itself
                float32v localDistance = distance[i];
                distance[i] = FS::Min( newDistance, distance[i] );
                newDistance = FS::Max( localDistance, newDistance );
                newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
            }
        }

        return this->ScaleOutput( this, FS::Convert<float>( this->DistanceAt( valueHash, valueIndex ) ), -kValueBounds, kValueBounds );
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

        std::array<int32v, COUNT> valueHash;
        std::array<float32v, COUNT> distance;
        
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

        typename CellularBase::template CellBounds<4> bounds;
        typename CellularBase::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, valueIndex, jitter, sizeJitterActive, sizeJitter );

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

            for( int i = 0; i < COUNT; i++ )
            {
                mask32v closer = newDistance < distance[i];

                int32v localCellValueHash = valueHash[i];
                valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                if( i >= valueIndex )
                {
                    distance[i] = FS::Select( closer, newDistance, distance[i] );
                    break;
                }

                // Same as selecting on closer (incl. ties), but SSE2 doesn't match these selects to min/max itself
                float32v localDistance = distance[i];
                distance[i] = FS::Min( newDistance, distance[i] );
                newDistance = FS::Max( localDistance, newDistance );
                newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
            }
        }

        return this->ScaleOutput( this, FS::Convert<float>( this->DistanceAt( valueHash, valueIndex ) ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularDistance, SIMD> final : public virtual CellularDistance, public DispatchClass<Cellular<>, SIMD>
{
    // Names the cellular base for its member templates (CellBounds, CellIterator); GCC 16 rejects reaching them through the injected class name
    using CellularBase = DispatchClass<Cellular<>, SIMD>;

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
    FS_FORCEINLINE void VisitCell2D( std::array<float32v, COUNT>& distance, DistanceFunction distFunc, float32v minkowskiP, float32v jitter, bool sizeJitterActive,
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

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter2D * mGridJitter.constant );

        this->ScalePositions( this, x, y );

        float32v pos[] = { x, y };
        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );
        distance[0] = this->template FloorCellsClosest<2, false, false>( distFunc, seed, jitter, pos, nullptr, nullptr );

        return GetReturn( distance, 1 + this->kJitter2D );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        if( this->template FloorCellsActive<2>( GetDistanceCount(), this->kJitter2D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y );
        }

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

        std::array<float32v, COUNT> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y );

        if constexpr( SPECIALISED && COUNT == 1 && !SIZE_JITTER && ( DIST_FUNC == DistanceFunction::EuclideanSquared || DIST_FUNC == DistanceFunction::Manhattan ) )
        {
            if( this->Closest2DActive( x, y, jitter ) )
            {
                distance[0] = this->template Closest2D<false, false>( distFunc, seed, jitter, x, y, nullptr, nullptr );
                return GetReturn( distance, 1 + this->kJitter2D );
            }
        }

        // 2D cells are cheap: only Minkowski's are expensive enough for pruning to pay off in 2D,
        // other distance functions measured slower with it on incoherent positions
        if( SPECIALISED || distFunc != DistanceFunction::Minkowski )
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
        else
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
            typename CellularBase::template CellBounds<2> bounds;
            typename CellularBase::template CellIterator<2> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

            while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
            {
                VisitCell2D<COUNT>( distance, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellOffset[0][o[0]], cellOffset[1][o[1]] );
            }
        }

        return GetReturn( distance, 1 + this->kJitter2D );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter3D * mGridJitter.constant );

        this->ScalePositions( this, x, y, z );

        float32v pos[] = { x, y, z };
        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );
        distance[0] = this->template FloorCellsClosest<3, false, false>( distFunc, seed, jitter, pos, nullptr, nullptr );

        return GetReturn( distance, 1 + this->kJitter3D );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        if( this->template FloorCellsActive<3>( GetDistanceCount(), this->kJitter3D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y, z );
        }

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

        std::array<float32v, COUNT> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( this, x, y, z );

        // The 2^D cells around the position usually hold the closest point, the 3^D search below only runs if a lane might be missing it
        if constexpr( SPECIALISED && COUNT == 1 && !SIZE_JITTER && ( DIST_FUNC == DistanceFunction::EuclideanSquared || DIST_FUNC == DistanceFunction::Manhattan ) )
        {
            float32v floorPos[] = { x, y, z };
            if( this->FloorFirst3DActive( distFunc, jitter, floorPos ) )
            {
                float32v closest = this->template FloorCellsClosest<3, false, false>( distFunc, seed, jitter, floorPos, nullptr, nullptr );
                distance[0] = closest;
                if( this->template FloorCellsComplete<3>( distFunc, closest, jitter, floorPos ) )
                {
                    return GetReturn( distance, 1 + this->kJitter3D );
                }
                // The 3^D search starts from the closest point so far, its pruning then skips most cells
            }
        }

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
        typename CellularBase::template CellBounds<3> bounds;
        typename CellularBase::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

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

        std::array<float32v, COUNT> distance;
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
        typename CellularBase::template CellBounds<4> bounds;
        typename CellularBase::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, minkowskiP, pruneIndex, jitter, sizeJitterActive, sizeJitter );

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

    template<size_t N>
    FS_FORCEINLINE float32v GetReturn( const std::array<float32v, N>& distance, float maxDist ) const
    {
        float32v distance0 = this->DistanceAt( distance, mDistanceIndex0 );
        bool euclidean = mDistanceFunction == FastNoise::DistanceFunction::Euclidean;

        if( euclidean )
        {
            distance0 = FastLengthSqrt( distance0 );
        }

        maxDist *= maxDist;

        if( mReturnType == ReturnType::Index0 )
        {
            return this->ScaleOutput( this, distance0, 0, maxDist );
        }

        // Index1 is only tracked when the return type uses it
        float32v distance1 = this->DistanceAt( distance, mDistanceIndex1 );
        if( euclidean )
        {
            distance1 = FastLengthSqrt( distance1 );
        }

        switch( mReturnType )
        {
        default:
        {
            return this->ScaleOutput( this, distance0, 0, maxDist );
        }
        case ReturnType::Index0Add1:
        {
            return this->ScaleOutput( this, distance0 + distance1, 0, maxDist * 2 );
        }
        case ReturnType::Index0Sub1:
        {
            return this->ScaleOutput( this, FS::Abs( distance0 - distance1 ), 0, maxDist );
        }
        case ReturnType::Index0Mul1:
        {
            return this->ScaleOutput( this, distance0 * distance1, 0, maxDist * maxDist );
        }
        case ReturnType::Index0Div1:
        {
            return this->ScaleOutput( this, distance0 * FS::Reciprocal( distance1 ), 0, maxDist );
        }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularLookup, SIMD> final : public virtual CellularLookup, public DispatchClass<Cellular<Seeded<ScalableGenerator>>, SIMD>
{
    // Names the cellular base for its member templates (CellBounds, CellIterator); GCC 16 rejects reaching them through the injected class name
    using CellularBase = DispatchClass<Cellular<Seeded<ScalableGenerator>>, SIMD>;

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

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter2D * mGridJitter.constant );

        this->ScalePositions( this, x, y );

        float32v pos[] = { x, y };
        float32v cell[2];
        this->template FloorCellsClosest<2, false, true>( distFunc, seed, jitter, pos, nullptr, cell );

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cell[0] * float32v( mScale ), cell[1] * float32v( mScale ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        if( this->template FloorCellsActive<2>( 1, this->kJitter2D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y );
        }

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

        if constexpr( SPECIALISED && !SIZE_JITTER && ( DIST_FUNC == DistanceFunction::EuclideanSquared || DIST_FUNC == DistanceFunction::Manhattan ) )
        {
            if( this->Closest2DActive( x, y, jitter ) )
            {
                float32v cell[2];
                this->template Closest2D<false, true>( distFunc, seed, jitter, x, y, nullptr, cell );
                return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cell[0] * float32v( mScale ), cell[1] * float32v( mScale ) );
            }
        }

        // 2D cells are cheap: only Minkowski's are expensive enough for pruning to pay off in 2D,
        // other distance functions measured slower with it on incoherent positions
        if( SPECIALISED || distFunc != DistanceFunction::Minkowski )
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
        else
        {
            float32v pos[] = { x, y };

            std::array<int32v, 3> cellPrimed[2];
            std::array<float32v, 3> cellPos[2];
            this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0] );
            this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );

            typename CellularBase::template CellBounds<2> bounds;
            typename CellularBase::template CellIterator<2> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

            while( const int8_t* o = cells.Next( distance ) )
            {
                VisitCell2D( distance, cellX, cellY, distFunc, minkowskiP, jitter, sizeJitterActive, sizeJitter, seed, x, y, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPos[0][o[0]], cellPos[1][o[1]] );
            }
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ) );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC>
    float32v FS_VECTORCALL GenFloorT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter( this->kJitter3D * mGridJitter.constant );

        this->ScalePositions( this, x, y, z );

        float32v pos[] = { x, y, z };
        float32v cell[3];
        this->template FloorCellsClosest<3, false, true>( distFunc, seed, jitter, pos, nullptr, cell );

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cell[0] * float32v( mScale ), cell[1] * float32v( mScale ), cell[2] * float32v( mScale ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        if( this->template FloorCellsActive<3>( 1, this->kJitter3D ) )
        {
            auto genFloor = this->SelectFloorGenT( []( auto specialised, auto distFunc ) -> GenTPtr<float32v, float32v, float32v>
            {
                return &DispatchClass::GenFloorT<decltype( specialised )::value, decltype( distFunc )::value>;
            } );

            return ( this->*genFloor )( seed, x, y, z );
        }

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

        // The 2^D cells around the position usually hold the closest point, the 3^D search below only runs if a lane might be missing it
        // Manhattan only: with EuclideanSquared the pruned 3^D search alone measured faster for CellularLookup
        if constexpr( SPECIALISED && !SIZE_JITTER && DIST_FUNC == DistanceFunction::Manhattan )
        {
            float32v floorPos[] = { x, y, z };
            if( this->template FloorFirst3DActive<false>( distFunc, jitter, floorPos ) )
            {
                float32v cell[3];
                float32v closest = this->template FloorCellsClosest<3, false, true>( distFunc, seed, jitter, floorPos, nullptr, cell );
                if( this->template FloorCellsComplete<3>( distFunc, closest, jitter, floorPos ) )
                {
                    return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cell[0] * float32v( mScale ), cell[1] * float32v( mScale ), cell[2] * float32v( mScale ) );
                }
            }
        }

        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellPos[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0], seed );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellPos[2] );

        typename CellularBase::template CellBounds<3> bounds;
        typename CellularBase::template CellIterator<3> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

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

        typename CellularBase::template CellBounds<4> bounds;
        typename CellularBase::template CellIterator<4> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, minkowskiP, 0, jitter, sizeJitterActive, sizeJitter );

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
