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
    // Without size jitter Euclidean finds the same closest cells as EuclideanSquared, so it shares those variants
    template<int MAX_COUNT, typename FUNC>
    FS_FORCEINLINE auto SelectGenT( int count, FUNC&& func ) const
    {
        bool sizeJitterActive = this->mSizeJitter.simdGeneratorPtr || this->mSizeJitter.constant != 0.0f;

        switch( this->mDistanceFunction )
        {
        case DistanceFunction::Euclidean:
            if( sizeJitterActive ) // 2D CellularValue compares sqrt distances with size jitter
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
    // Per axis index 1 is the centre cell, 0 and 2 the sides: [-1, +1], or [near, far] per lane after NearFirst()
    // Linear: x-major over [-1, 0, +1], the original order, used when not pruning
    template<int D>
    struct CellOrder
    {
        static constexpr int kCount = D == 2 ? 9 : ( D == 3 ? 27 : 81 );
        int8_t nearest[kCount][D];
        int8_t linear[kCount][D];
        int8_t pairIndex[kCount][2]; // CellBounds pair indices for nearest order
        uint32_t parentMask[kCount]; // Nearest order: bits of the previous shell cells one step closer to the centre
        int shellStart[D + 2];

        constexpr CellOrder() : nearest{}, linear{}, pairIndex{}, parentMask{}, shellStart{}
        {
            int n = 0;
            for( int shell = 0; shell <= D; shell++ )
            {
                shellStart[shell] = n;
                for( int i = 0; i < kCount; i++ )
                {
                    int offCentre = 0;
                    for( int d = D - 1, c = i; d >= 0; d--, c /= 3 )
                    {
                        offCentre += c % 3 != 1;
                        linear[i][d] = (int8_t)( c % 3 );
                    }
                    if( offCentre == shell )
                    {
                        for( int d = D - 1, c = i; d >= 0; d--, c /= 3 )
                        {
                            nearest[n][d] = (int8_t)( c % 3 );
                        }
                        pairIndex[n][0] = (int8_t)( nearest[n][0] * 3 + nearest[n][1] );
                        pairIndex[n][1] = (int8_t)( D == 3 ? nearest[n][2] : nearest[n][2] * 3 + nearest[n][3] );
                        n++;
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
        static_assert( D == 3 || D == 4 );

        std::array<float32v, 3> axis[D];
        float32v pairA[D == 4 ? 9 : 1]; // 4D only: axes 0, 1 precombined
        float32v pairB[D == 4 ? 9 : 1]; // 4D only: axes 2, 3 precombined
    };

    enum PruneMode
    {
        kPruneNone,
        kPruneCoherent, // Lanes close together, the same cells are skippable in every lane
        kPruneNearFirst // Incoherent lanes, cells are ordered per lane relative to the near side (see NearFirst)
    };

    // Minkowski isn't pruned: approximate Pow and odd P values make bounds unreliable
    // 3D needing the 4th closest point skips too few cells to pay for the bound tests
    // Incoherent lanes always prune in 4D, in 3D only when they need the nearest 2 points with grid jitter <= 1,
    // the far side cells then rarely matter in any lane
    template<int D>
    FS_FORCEINLINE static PruneMode GetPruneMode( DistanceFunction distFunc, const float32v* pos, float32v jitter, float maxJitter, int distanceIndex )
    {
        if( distFunc == DistanceFunction::Minkowski || ( D == 3 && distanceIndex >= 3 ) )
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
            return kPruneCoherent;
        }
        if( D == 4 || ( distanceIndex <= 1 && AllLanes( FS::Abs( jitter ) <= float32v( maxJitter * 1.001f ) ) ) )
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
    FS_FORCEINLINE static void CellAxis( int32v cell, int prime, float32v origin, std::array<int32v, 3>& primed, std::array<float32v, 3>& cellPos )
    {
        float32v cellf = FS::Convert<float>( cell ) - origin;
        cell *= int32v( prime );

        for( int i = 0; i < 3; i++ )
        {
            primed[i] = cell;
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
                                     DistanceFunction distFunc, int distanceIndex, float32v jitter, bool sizeJitterActive, const float32v& sizeJitter ) :
            mBounds( bounds )
        {
            PruneMode pruneMode = GetPruneMode<D>( distFunc, pos, jitter, D == 3 ? kJitter3D : kJitter4D, distanceIndex );
            mPrune = pruneMode != kPruneNone;
            mOrder = mPrune ? kOrder.nearest : kOrder.linear;

            if( pruneMode == kPruneNearFirst )
            {
                NearFirst<D>( cellPrimed, cellPos, pos );
            }
            if( mPrune )
            {
                std::array<float32v, 3> centreOffset[D];
                for( int d = 0; d < D; d++ )
                {
                    for( int i = 0; i < 3; i++ )
                    {
                        centreOffset[d][i] = relative ? cellPos[d][i] : cellPos[d][i] - pos[d];
                    }
                }
                InitBounds( centreOffset, pos, distFunc, jitter, sizeJitterActive, sizeJitter );
            }
        }

        // Next cell's per axis slots, nullptr when done
        FS_FORCEINLINE const int8_t* Next( float32v distToBeat )
        {
            while( ++mCell < kOrder.kCount )
            {
                if( mPrune && mCell == kOrder.shellStart[mShell + 1] )
                {
                    mShell++;
                    mVisit = mShellVisit = ShellVisitMask( distToBeat, ~mShellVisit );
                }

                bool visit = !mPrune || ( mVisit & 1 );
                mVisit >>= 1;
                if( visit )
                {
                    return mOrder[mCell];
                }
            }
            return nullptr;
        }

    private:
        // A feature point is within |jitter| of its cell centre, so per axis |delta| >= max( 0, |centre - pos| - |jitter| )
        // centreOffset[d][i]: cell centre - position for cell slot i on axis d, see CellOrder
        FS_FORCEINLINE void InitBounds( const std::array<float32v, 3>* centreOffset, const float32v* pos, DistanceFunction distFunc,
                                        float32v jitter, bool sizeJitterActive, const float32v& sizeJitter )
        {
            // Slack for approximate InvSqrt in the jitter offset and float rounding of positions/distances
            float32v radius = FS::FMulAdd( FS::Abs( jitter ), float32v( 1 + 1.f / 512 ), float32v( 1.f / ( 1 << 20 ) ) );
            float32v scale( 1 - 1.f / 4096 );
            float32v minFactor;

            if( sizeJitterActive )
            {
                minFactor = FS::Min( FS::FNMulAdd( sizeJitter, float32v( D == 3 ? 0xffff : 0xfffff ), float32v( 1 ) ), float32v( 1 ) );
                scale *= minFactor;
            }

            mCombineMax = distFunc == DistanceFunction::MaxAxis;

            // Euclidean: the feature point is within a ball around the cell centre, much tighter than per axis bounds for edge/corner cells
            mBall = !sizeJitterActive && ( distFunc == DistanceFunction::Euclidean || distFunc == DistanceFunction::EuclideanSquared );

            if( mBall )
            {
                float32v posSlack = FS::Abs( pos[0] );
                for( int d = 1; d < D; d++ )
                {
                    posSlack += FS::Abs( pos[d] );
                }
                mBallRadius = FS::FMulAdd( posSlack, float32v( 1.f / ( 1 << 21 ) ), radius );
            }

            for( int d = 0; d < D; d++ )
            {
                float32v axisRadius = FS::FMulAdd( FS::Abs( pos[d] ), float32v( 1.f / ( 1 << 21 ) ), radius );

                for( int i = 0; i < 3; i++ )
                {
                    if( mBall )
                    {
                        mBounds.axis[d][i] = centreOffset[d][i] * centreOffset[d][i];
                        continue;
                    }

                    float32v b = FS::Max( FS::Abs( centreOffset[d][i] ) - axisRadius, float32v( 0 ) );

                    switch( distFunc )
                    {
                    default:
                        b *= b;
                        break;
                    case DistanceFunction::Manhattan:
                    case DistanceFunction::MaxAxis:
                        break;
                    case DistanceFunction::Hybrid:
                        b = FS::FMulAdd( b, b, b );
                        break;
                    }

                    mBounds.axis[d][i] = b * scale;
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
                        mBounds.axis[d][i] = FS::Select( valid, mBounds.axis[d][i], float32v( -kInfinity ) );
                    }
                }
            }

            if constexpr( D == 4 )
            {
                // Precombine axis pairs so each cell's bound is a single add/max
                for( int i = 0; i < 9; i++ )
                {
                    mBounds.pairA[i] = Combine( mBounds.axis[0][i / 3], mBounds.axis[1][i % 3] );
                    mBounds.pairB[i] = Combine( mBounds.axis[2][i / 3], mBounds.axis[3][i % 3] );
                }
            }
        }

        FS_FORCEINLINE float32v Combine( float32v a, float32v b ) const
        {
            return mCombineMax ? FS::Max( a, b ) : a + b;
        }

        FS_FORCEINLINE float32v Bound( int cell ) const
        {
            if constexpr( D == 4 )
            {
                const int8_t* index = kOrder.pairIndex[cell];
                return Combine( mBounds.pairA[index[0]], mBounds.pairB[index[1]] );
            }
            else
            {
                const int8_t* index = kOrder.nearest[cell];
                return Combine( Combine( mBounds.axis[0][index[0]], mBounds.axis[1][index[1]] ), mBounds.axis[2][index[2]] );
            }
        }

        // Bit per cell in the current shell, set if any lane might find a feature point closer than distToBeat in it.
        // Tested per shell rather than per cell so branches don't wait on the previous cell's distance.
        // Bounds only grow moving away from the centre cell, so cells next to an already skipped cell are skipped untested
        FS_FORCEINLINE uint32_t ShellVisitMask( float32v distToBeat, uint32_t skippedPrev ) const
        {
            if( mBall )
            {
                // Ball bounds hold squared centre distances: distance >= ( |centre - pos| - ballRadius )^2
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
        const int8_t ( *mOrder )[D];
        bool mPrune;
        bool mBall = false;
        bool mCombineMax = false;
        int mCell = -1;
        int mShell = 0;
        uint32_t mVisit = ~0u;
        uint32_t mShellVisit = ~0u;
    };
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularValue, SIMD> final : public virtual CellularValue, public DispatchClass<Cellular<>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
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

        this->ScalePositions( x, y );

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

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
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

        this->ScalePositions( x, y, z );

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;
        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellOffset[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, x, cellPrimed[0], cellOffset[0] );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, y, cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, z, cellPrimed[2], cellOffset[2] );

        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, valueIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, valueIndex ) ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
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

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
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

        this->ScalePositions( x, y, z, w );

        int valueIndex = SPECIALISED ? COUNT - 1 : mValueIndex;
        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellOffset[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, x, cellPrimed[0], cellOffset[0] );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, y, cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, z, cellPrimed[2], cellOffset[2] );
        this->CellAxis( FS::Convert<int32_t>( w ) + int32v( -1 ), Primes::W, w, cellPrimed[3], cellOffset[3] );

        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, valueIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, valueIndex ) ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
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

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularDistance, SIMD> final : public virtual CellularDistance, public DispatchClass<Cellular<>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    // Number of closest distances to track, 3 is rounded up to use the specialised 4 variant
    int GetDistanceCount() const
    {
        int count = std::max( mDistanceIndex0, mReturnType == ReturnType::Index0 ? 0 : mDistanceIndex1 ) + 1;
        return count == 3 ? kMaxDistanceCount : count;
    }

    // Largest distance index the output depends on, cells that can't beat it are skipped
    template<int COUNT>
    FS_FORCEINLINE int PruneDistanceIndex() const
    {
        if constexpr( COUNT <= 2 ) // Not rounded up, compile time constant
        {
            return COUNT - 1;
        }
        return mReturnType == ReturnType::Index0 ? mDistanceIndex0 : std::max( mDistanceIndex0, mDistanceIndex1 );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
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

        this->ScalePositions( x, y );

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
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcfOffset );
                yd = FS::FMulAdd( yd, invMag, ycf - y );

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

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance, 1 + this->kJitter2D );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
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

        this->ScalePositions( x, y, z );

        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellOffset[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellOffset[0] );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellOffset[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellOffset[2] );
        for( int d = 0; d < 3; d++ )
        {
            for( int i = 0; i < 3; i++ )
            {
                cellOffset[d][i] -= pos[d];
            }
        }

        int pruneIndex = PruneDistanceIndex<COUNT>();
        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, pruneIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
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

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
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

        this->ScalePositions( x, y, z, w );

        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellOffset[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellOffset[0] );
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

        int pruneIndex = PruneDistanceIndex<COUNT>();
        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellOffset, true, pos, distFunc, pruneIndex, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( this->DistanceAt( distance, pruneIndex ) ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
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
            return this->ScaleOutput( distance[mDistanceIndex0], 0, maxDist );
        }
        case ReturnType::Index0Add1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] + distance[mDistanceIndex1], 0, maxDist * 2 );
        }
        case ReturnType::Index0Sub1:
        {
            return this->ScaleOutput( FS::Abs( distance[mDistanceIndex0] - distance[mDistanceIndex1] ), 0, maxDist );
        }
        case ReturnType::Index0Mul1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] * distance[mDistanceIndex1], 0, maxDist * maxDist );
        }
        case ReturnType::Index0Div1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] * FS::Reciprocal( distance[mDistanceIndex1] ), 0, maxDist );
        }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularLookup, SIMD> final : public virtual CellularLookup, public DispatchClass<Cellular<Seeded<ScalableGenerator>>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
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

        this->ScalePositions( x, y );

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
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                float32v localCellX = FS::FMulAdd( xd, invMag, xcf );
                float32v localCellY = FS::FMulAdd( yd, invMag, ycf );
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

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
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

        this->ScalePositions( x, y, z );

        float32v pos[] = { x, y, z };

        std::array<int32v, 3> cellPrimed[3];
        std::array<float32v, 3> cellPos[3];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0] );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellPos[2] );

        typename DispatchClass::template CellBounds<3> bounds;
        typename DispatchClass::template CellIterator<3> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, 0, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( distance ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]] );
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

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<1>( 1, []( auto specialised, auto distFunc, auto count, auto sizeJitter ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value, decltype( sizeJitter )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT, bool SIZE_JITTER>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
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

        this->ScalePositions( x, y, z, w );

        float32v pos[] = { x, y, z, w };

        std::array<int32v, 3> cellPrimed[4];
        std::array<float32v, 3> cellPos[4];
        this->CellAxis( FS::Convert<int32_t>( x ) + int32v( -1 ), Primes::X, float32v( 0 ), cellPrimed[0], cellPos[0] );
        this->CellAxis( FS::Convert<int32_t>( y ) + int32v( -1 ), Primes::Y, float32v( 0 ), cellPrimed[1], cellPos[1] );
        this->CellAxis( FS::Convert<int32_t>( z ) + int32v( -1 ), Primes::Z, float32v( 0 ), cellPrimed[2], cellPos[2] );
        this->CellAxis( FS::Convert<int32_t>( w ) + int32v( -1 ), Primes::W, float32v( 0 ), cellPrimed[3], cellPos[3] );

        typename DispatchClass::template CellBounds<4> bounds;
        typename DispatchClass::template CellIterator<4> cells( bounds, cellPrimed, cellPos, false, pos, distFunc, 0, jitter, sizeJitterActive, sizeJitter );

        while( const int8_t* o = cells.Next( distance ) )
        {
            int32v hash = HashPrimesHB( seed, cellPrimed[0][o[0]], cellPrimed[1][o[1]], cellPrimed[2][o[2]], cellPrimed[3][o[3]] );
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
