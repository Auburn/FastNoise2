#ifndef FASTNOISE_EXPORT_H
#define FASTNOISE_EXPORT_H

#if ( !defined( FASTNOISE_STATIC_LIB ) && !defined( FASTSIMD_STATIC_LIB ) ) && ( defined( _WIN32 ) || defined( __CYGWIN__ ) )
    #if defined( FASTNOISE_EXPORT ) || defined( FASTSIMD_EXPORT )
        #define FASTNOISE_API __declspec( dllexport )
    #else
        #define FASTNOISE_API __declspec( dllimport )
    #endif
#else
    #define FASTNOISE_API
#endif

#if defined( FASTNOISE_STATIC_LIB ) && !defined( FASTSIMD_STATIC_LIB )
    #define FASTSIMD_STATIC_LIB
#endif

#endif