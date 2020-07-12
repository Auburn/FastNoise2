#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainScale : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetScale( float value ) { mScale = value; }

    protected:
        GeneratorSource mSource;
        float mScale = 1.0f;

        FASTNOISE_METADATA( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddGeneratorSource( "Source", &DomainScale::SetSource );
                this->AddVariable( "Scale", 1.0f, &DomainScale::SetScale );
            }
        };    
    };

    class DomainOffset : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetOffsetX( float value ) { mOffsetX = value; }
        void SetOffsetX( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mOffsetX, gen ); }
        void SetOffsetY( float value ) { mOffsetY = value; }
        void SetOffsetY( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mOffsetY, gen ); }
        void SetOffsetZ( float value ) { mOffsetZ = value; }
        void SetOffsetZ( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mOffsetZ, gen ); }
        void SetOffsetW( float value ) { mOffsetW = value; }
        void SetOffsetW( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mOffsetW, gen ); }

    protected:
        GeneratorSource mSource;
        HybridSource mOffsetX, mOffsetY, mOffsetZ, mOffsetW;

        FASTNOISE_METADATA( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddGeneratorSource( "Source", &DomainOffset::SetSource );
                this->AddHybridSource( "Offset X", 0.0f, &DomainOffset::SetOffsetX, &DomainOffset::SetOffsetX );
                this->AddHybridSource( "Offset Y", 0.0f, &DomainOffset::SetOffsetY, &DomainOffset::SetOffsetY );
                this->AddHybridSource( "Offset Z", 0.0f, &DomainOffset::SetOffsetZ, &DomainOffset::SetOffsetZ );
                this->AddHybridSource( "Offset W", 0.0f, &DomainOffset::SetOffsetW, &DomainOffset::SetOffsetW );
            }
        };    
    };

    class SeedOffset : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetOffset( int32_t value ) { mOffset = value; }

    protected:
        GeneratorSource mSource;
        int32_t mOffset = 1;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
        {
            this->AddGeneratorSource( "Source", &SeedOffset::SetSource );
            this->AddVariable( "Seed Offset", 1, &SeedOffset::SetOffset );
        }
    };
    };

    class Remap : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetRemap( float fromMin, float fromMax, float toMin, float toMax ) { mFromMin = fromMin; mFromMax = fromMax; mToMin = toMin; mToMax = toMax; }

    protected:
        GeneratorSource mSource;
        float mFromMin = -1.0f;
        float mFromMax = 1.0f;
        float mToMin = 0.0f;
        float mToMax = 1.0f;

        FASTNOISE_METADATA( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddGeneratorSource( "Source", &Remap::SetSource );

                this->AddVariable( "From Min", -1.0f,
                    []( Remap* p, float f )
                {
                    p->mFromMin = f;
                });
                
                this->AddVariable( "From Max", 1.0f,
                    []( Remap* p, float f )
                {
                    p->mFromMax = f;
                });
                
                this->AddVariable( "To Min", 0.0f,
                    []( Remap* p, float f )
                {
                    p->mToMin = f;
                });

                this->AddVariable( "To Max", 1.0f,
                    []( Remap* p, float f )
                {
                    p->mToMax = f;
                });
            }
        };    
    };

    class ConvertRGBA8 : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetMinMax( float min, float max ) { mMin = min; mMax = max; }

    protected:
        GeneratorSource mSource;
        float mMin = -1.0f;
        float mMax = 1.0f;

        FASTNOISE_METADATA( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {            
                this->AddGeneratorSource( "Source", &ConvertRGBA8::SetSource );

                 this->AddVariable( "Min", -1.0f,
                    []( ConvertRGBA8* p, float f )
                {
                    p->mMin = f;
                });

                this->AddVariable( "Max", 1.0f,
                    []( ConvertRGBA8* p, float f )
                {
                    p->mMax = f;
                });
            }
        };    
    };
}
