#include "FastSIMD/InlInclude.h"

#include "Modifiers.h"

template<typename FS>
class FS_T<FastNoise::DomainScale, FS> : public virtual FastNoise::DomainScale, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    float32v FS_INLINE GenT( int32v seed, P... pos )
    {
        return this->GetSourceValue( mSource, seed, (pos * float32v( mScale ))... );
    }
};

template<typename FS>
class FS_T<FastNoise::Remap, FS> : public virtual FastNoise::Remap, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    float32v FS_INLINE GenT( int32v seed, P... pos )
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
            
        return float32v( mToMin ) + (( source - float32v( mFromMin ) ) / float32v( mFromMax - mFromMin ) * float32v( mToMax - mToMin ));
    }
};

template<typename FS>
class FS_T<FastNoise::ConvertRGBA8, FS> : public virtual FastNoise::ConvertRGBA8, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    float32v FS_INLINE GenT( int32v seed, P... pos )
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
        
        source = FS_Min_f32( source, float32v( mMax ));
        source = FS_Max_f32( source, float32v( mMin ));
        source -= float32v( mMin );

        source *= float32v( 255.0f / (mMax - mMin) );

        int32v byteVal = FS_Convertf32_i32( source );

        int32v output = int32v( 255 << 24 );
        output |= byteVal;
        output |= byteVal << 8;
        output |= byteVal << 16;

        return FS_Casti32_f32( output );
    }
};

