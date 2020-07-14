#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Constant : public virtual Generator
    {
    public:
        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddVariable( "Value", 1.0f, &Constant::SetValue );
            }
        };
    };

    class Checkerboard : public virtual Generator
    {
    public:
        void SetSize( float value ) { mSize = value; }

    protected:
        float mSize = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddVariable( "Size", 1.0f, &Checkerboard::SetSize );
            }
        };
    };

    class SineWave : public virtual Generator
    {
    public:
        void SetScale( float value ) { mScale = value; }

    protected:
        float mScale = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddVariable( "Scale", 1.0f, &SineWave::SetScale );
            }
        };
    };

    class PositionOutput : public virtual Generator
    {
    public:
        void SetX( float multiplier, float offset = 0.0f ) { mMultiplierX = multiplier; mOffsetX = offset; }
        void SetY( float multiplier, float offset = 0.0f ) { mMultiplierY = multiplier; mOffsetY = offset; }
        void SetZ( float multiplier, float offset = 0.0f ) { mMultiplierZ = multiplier; mOffsetZ = offset; }
        void SetW( float multiplier, float offset = 0.0f ) { mMultiplierW = multiplier; mOffsetW = offset; }

    protected:
        float mMultiplierX = 0.0f;
        float mOffsetX = 0.0f;

        float mMultiplierY = 0.0f;
        float mOffsetY = 0.0f;

        float mMultiplierZ = 0.0f;
        float mOffsetZ = 0.0f;

        float mMultiplierW = 0.0f;
        float mOffsetW = 0.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddVariable( "X Multiplier", 0.0f, []( PositionOutput* p, float f ) { p->mMultiplierX = f; } );
                this->AddVariable( "X Offset", 0.0f, []( PositionOutput* p, float f ) { p->mOffsetX = f; } );
                this->AddVariable( "Y Multiplier", 0.0f, []( PositionOutput* p, float f ) { p->mMultiplierY = f; } );
                this->AddVariable( "Y Offset", 0.0f, []( PositionOutput* p, float f ) { p->mOffsetY = f; } );
                this->AddVariable( "Z Multiplier", 0.0f, []( PositionOutput* p, float f ) { p->mMultiplierZ = f; } );
                this->AddVariable( "Z Offset", 0.0f, []( PositionOutput* p, float f ) { p->mOffsetZ = f; } );
                this->AddVariable( "W Multiplier", 0.0f, []( PositionOutput* p, float f ) { p->mMultiplierW = f; } );
                this->AddVariable( "W Offset", 0.0f, []( PositionOutput* p, float f ) { p->mOffsetW = f; } );
            }
        };
    };
}
