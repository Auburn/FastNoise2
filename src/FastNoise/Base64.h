#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FastNoise
{
    namespace Base64
    {
        static std::string Encode( const std::vector<uint8_t>& data )
        {
            static constexpr char kEncodingTable[] = {
                'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                '4', '5', '6', '7', '8', '9', '+', '/'
            };

            size_t inLen = data.size();
            std::string ret;
            size_t consecutiveAs = 0;

            auto appendChar = [&]( char c ) {
                if( c == 'A' ) // Compress "A"s into @ with count in following char
                {
                    if( consecutiveAs++ <= 1 )
                    {
                        ret += 'A';
                    }
                    else if( consecutiveAs >= std::size( kEncodingTable ) + 2 )
                    {
                        ret[ret.length() - 2] = '@';
                        ret[ret.length() - 1] = kEncodingTable[consecutiveAs - 3];

                        ret += 'A';
                        consecutiveAs = 1;
                    }
                }
                else
                {
                    if( consecutiveAs >= 3 )
                    {
                        ret[ret.length() - 2] = '@';
                        ret[ret.length() - 1] = kEncodingTable[consecutiveAs - 3];
                    }
                    if( c != '\0' )
                    {
                        ret += c;
                    }

                    consecutiveAs = 0;
                }
            };

            size_t i;

            for( i = 0; i < inLen - 2; i += 3 )
            {
                appendChar( kEncodingTable[( data[i] >> 2 ) & 0x3F] );
                appendChar( kEncodingTable[( ( data[i] & 0x3 ) << 4 ) | ( ( data[i + 1] & 0xF0 ) >> 4 )] );
                appendChar( kEncodingTable[( ( data[i + 1] & 0xF ) << 2 ) | ( ( data[i + 2] & 0xC0 ) >> 6 )] );
                appendChar( kEncodingTable[data[i + 2] & 0x3F] );
            }
            if( i < inLen )
            {
                appendChar( kEncodingTable[( data[i] >> 2 ) & 0x3F] );
                if( i == ( inLen - 1 ) )
                {
                    appendChar( kEncodingTable[( ( data[i] & 0x3 ) << 4 )] );
                    appendChar( '=' );
                }
                else
                {
                    appendChar( kEncodingTable[( ( data[i] & 0x3 ) << 4 ) | ( ( data[i + 1] & 0xF0 ) >> 4 )] );
                    appendChar( kEncodingTable[( ( data[i + 1] & 0xF ) << 2 )] );
                }
                appendChar( '=' );
            }
            else
            {
                // Handle any trailing As
                appendChar( '\0' );
            }

            return ret;
        }

        static std::vector<uint8_t> Decode( const char* input )
        {
            static constexpr unsigned char kDecodingTable[] = {
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 0, 64, 64,
                64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
                64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
            };

            size_t rawLen = 0, decompLen = 0;

            // Check string length with decompress
            while( input[rawLen] )
            {
                if( input[rawLen] == '@' )
                {
                    unsigned char aExtra = kDecodingTable[static_cast<unsigned char>( input[++rawLen] )];

                    if( aExtra == 64 ) // Error
                    {
                        return {};
                    }

                    decompLen += aExtra + 2;
                }
                else
                {
                    decompLen++;
                    rawLen++;
                }
            }

            size_t outLen = decompLen / 4 * 3;

            if( outLen == 0 || decompLen % 4 != 0 )
                return {};
                        
            if( input[rawLen - 1] == '=' )
            {
                outLen--;
                if( input[rawLen - 2] == '=' )
                    outLen--;
            }

            std::vector<uint8_t> out( outLen );
            size_t i = 0, j = 0, consecutiveAs = 0;

            while( i < rawLen || consecutiveAs > 0 )
            {
                char currentBlock[4] = { 0 };

                for( int k = 0; k < 4; k++ )
                {
                    if( consecutiveAs > 0 )
                    {
                        currentBlock[k] = 'A';
                        consecutiveAs--;
                    }
                    else if( input[i] == '@' )
                    {
                        currentBlock[k] = 'A';
                        i++;
                        consecutiveAs = kDecodingTable[static_cast<unsigned char>( input[i++] )] + 2;
                    }
                    else
                    {
                        currentBlock[k] = input[i++];
                    }
                }

                uint32_t a = kDecodingTable[static_cast<unsigned char>( currentBlock[0] )];
                uint32_t b = kDecodingTable[static_cast<unsigned char>( currentBlock[1] )];
                uint32_t c = kDecodingTable[static_cast<unsigned char>( currentBlock[2] )];
                uint32_t d = kDecodingTable[static_cast<unsigned char>( currentBlock[3] )];

                uint32_t triple = ( a << 3 * 6 ) + ( b << 2 * 6 ) + ( c << 1 * 6 ) + ( d << 0 * 6 );

                if( j < outLen )
                    out[j++] = ( triple >> 2 * 8 ) & 0xFF;
                if( j < outLen )
                    out[j++] = ( triple >> 1 * 8 ) & 0xFF;
                if( j < outLen )
                    out[j++] = ( triple >> 0 * 8 ) & 0xFF;
            }

            return out;
        }
    }; // namespace Base64
} // namespace FastNoise
