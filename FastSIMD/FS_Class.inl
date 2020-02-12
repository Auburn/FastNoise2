#undef FASTSIMD_CLASS_DECLARATION
#undef FASTSIMD_CLASS_DECLARATION_CHILD
#undef FASTSIMD_CLASS_SETUP
#undef FASTSIMD_DEFINE_DOWNCAST_FUNC
#undef FS_EXTERNAL_FUNC
#undef FS_EXTERNAL
#undef FS_INTERNAL

#include "FastSIMD.h"

#if ( !defined( FASTSIMD_INCLUDE_CHECK ) || !defined( FS_SIMD_CLASS ) ) && ( !defined( __INTELLISENSE__ ) || !defined( FASTSIMD_INTELLISENSE ) )

#define FASTSIMD_CLASS_DECLARATION( CLASS ) \
class CLASS

#define FASTSIMD_CLASS_DECLARATION_CHILD( CLASS, CHILD ) \
class CLASS : public virtual CHILD

#define FASTSIMD_CLASS_SETUP( ... )\
public: virtual FastSIMD::ELevel GetSIMDLevel() = 0;\
static const FastSIMD::Level_BitFlags Supported_SIMD_Levels = ( __VA_ARGS__ );\
static_assert( (Supported_SIMD_Levels & FastSIMD::FASTSIMD_FALLBACK_SIMD_LEVEL) != 0, "FASTSIMD_FALLBACK_SIMD_LEVEL must be supported" );


#define FASTSIMD_DEFINE_DOWNCAST_FUNC( CLASS ) \
public: virtual void* GetPtrSIMD_ ## CLASS() = 0

#define FS_EXTERNAL_FUNC( ... ) virtual __VA_ARGS__ = 0
#define FS_EXTERNAL( ... ) __VA_ARGS__
#define FS_INTERNAL( ... )

#if defined( FS_SIMD_CLASS )
#define FASTSIMD_INCLUDE_CHECK
#endif

#else

#include "Internal/FunctionList.h"

#undef FASTSIMD_INCLUDE_CHECK

#define FASTSIMD_CLASS_DECLARATION( CLASS ) \
template<typename T_FS, FastSIMD::ELevel = T_FS::SIMD_Level> class FS_CLASS( CLASS ) : public virtual CLASS

#define FASTSIMD_CLASS_DECLARATION_CHILD( CLASS, CHILD ) \
template<typename T_FS, FastSIMD::ELevel = T_FS::SIMD_Level> class FS_CLASS( CLASS ) : public virtual CLASS, public FS_CLASS( CHILD )<T_FS> 

#define FASTSIMD_CLASS_SETUP( ... ) \
private:\
typedef typename T_FS FS;\
typedef typename FS::float32v float32v;\
typedef typename FS::int32v int32v;\
typedef typename FS::mask32v mask32v;\
public: FastSIMD::ELevel GetSIMDLevel() override { return FS::SIMD_Level; }

#define FASTSIMD_DEFINE_DOWNCAST_FUNC( CLASS ) \
public: void* GetPtrSIMD_ ## CLASS() final { return (FS_CLASS( CLASS )<FS>*)this; } \
public: static FS_CLASS( CLASS )<FS>* GetSIMD_ ## CLASS( CLASS* base ) \
{ if ( base && base->GetSIMDLevel() == FS::SIMD_Level ) return reinterpret_cast<FS_CLASS( CLASS )<FS>*>( base->GetPtrSIMD_ ## CLASS() ); \
    return nullptr; } 

#define FS_EXTERNAL_FUNC( ... ) __VA_ARGS__ override
#define FS_EXTERNAL( ... ) 
#define FS_INTERNAL( ... ) __VA_ARGS__

#endif

