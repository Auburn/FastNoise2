#include <filesystem>
#include <FastNoise/Metadata.h>
#include <FastNoise/FastNoise.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <unordered_map>

static constexpr int imageSizeX = 256;
static constexpr int imageSizeY = 256;

FastNoise::SmartNode<> BuildGenerator( const FastNoise::Metadata* metadata )
{
    FastNoise::SmartNode<> generator = metadata->CreateNode();

    auto source = FastNoise::New<FastNoise::Constant>();
    source->SetValue( 0.5f );

    for( const auto& memberNode : metadata->memberNodeLookups )
    {
        if( !memberNode.setFunc( generator.get(), source ) )
        {
            // If constant source is not valid try all other node types in order
            for( const FastNoise::Metadata* tryMetadata : FastNoise::Metadata::GetAll() )
            {
                // Other node types may also have sources
                FastNoise::SmartNode<> trySource = BuildGenerator( tryMetadata );

                if( trySource && memberNode.setFunc( generator.get(), trySource ) )
                {
                    for( const auto& tryMemberNode : tryMetadata->memberNodeLookups )
                    {
                        if( !tryMemberNode.setFunc( trySource.get(), source ) )
                        {
                            return {};
                        }
                    }
                    break;
                }
            }
        }
    }
    return generator;
}

bool CreateImage( const FastNoise::Metadata* metadata, const std::string& outDir, const std::string& nodeName )
{
    auto node = FastNoise::New<FastNoise::DomainScale>();
    node->SetSource( BuildGenerator( metadata ) );
    node->SetScaling( 3.f );

    std::vector<float> noiseData( imageSizeX * imageSizeY );
    auto noiseMinMax = node->GenUniformGrid2D( noiseData.data(), imageSizeX / -2, imageSizeY / -2, imageSizeX, imageSizeY, 1337 );

    if( noiseMinMax.min == noiseMinMax.max || !std::isfinite( noiseMinMax.min  ) || !std::isfinite( noiseMinMax.max ) )
    {
        return false;
    }

    std::filesystem::path tempFile = std::filesystem::temp_directory_path() / (nodeName + ".bmp");

    std::ofstream file( tempFile, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );

    if( file.is_open() )
    {
        float scale = 255 / (noiseMinMax.max - noiseMinMax.min);

        struct BmpHeader
        {
            // File header (14)
            // char b = 'B';
            // char m = 'M';
            uint32_t fileSize;
            uint32_t reserved = 0;
            uint32_t dataOffset = 14u + 12u + (256u * 3u);
            // Bmp Info Header (12)
            uint32_t headerSize = 12u;
            uint16_t sizeX;
            uint16_t sizeY;
            uint16_t colorPlanes = 1u;
            uint16_t bitDepth = 8u;
        };

        int paddedSizeX = imageSizeX;
        int padding = paddedSizeX % 4;
        if( padding )
        {
            padding = 4 - padding;
            paddedSizeX += padding;
        }

        BmpHeader header;
        header.fileSize = header.dataOffset + (uint32_t)(paddedSizeX * imageSizeY);
        header.sizeX = (uint16_t)imageSizeX;
        header.sizeY = (uint16_t)imageSizeY;

        file << 'B' << 'M';
        file.write( reinterpret_cast<char*>( &header ), sizeof( BmpHeader ) );

        // Colour map
        for (int i = 0; i < 256; i++)
        {
            char colourB = i;
            file.write( &colourB, 1 );
            file.write( &colourB, 1 );
            file.write( &colourB, 1 );
        }

        int xIdx = padding ? imageSizeX : 0;

        for( float noise : noiseData )
        {
            unsigned char pix = (unsigned char)std::clamp( (noise - noiseMinMax.min) * scale, 0.0f, 255.0f );

            file.write( reinterpret_cast<char*>( &pix ), 1 );

            if( --xIdx == 0 )
            {
                xIdx = imageSizeX;

                int zero( 0 );
                file.write( reinterpret_cast<char*>( &zero ), padding );
            }
        }

        file.close();

        std::string convertCmd = "magick convert \"";
        convertCmd += tempFile.string();
        convertCmd += "\" \"" + outDir + "/images/" + nodeName + ".png";

        std::system( convertCmd.c_str() );
        return true;
    }
    return false;
}

std::string FormatDescription( const char* description )
{
    std::string formatted = description;
    size_t pos = 0;
    
    while( (pos = formatted.find( '\n', pos )) != std::string::npos )
    {
        formatted.insert( pos, "<br/>" );
        pos += 6; // Length of "\n<br/>"
    }

    return formatted;
}

void DoNode( std::stringstream& output, const FastNoise::Metadata* metadata, const std::string& outDir )
{
    std::string nodeName = FastNoise::Metadata::FormatMetadataNodeName( metadata, false );

    output << "## " << nodeName << '\n';
    output << FormatDescription( metadata->description ) << "\n\n";

    if( CreateImage( metadata, outDir, nodeName ) )
    {
        output << "[[images/" << nodeName << ".png]]\n";
    }

    for( auto& node_lookup : metadata->memberNodeLookups )
    {
        output << "### " << node_lookup.name << " _- Node Lookup_\n" << FormatDescription( node_lookup.description ) << '\n';
    }

    for( auto& hybrid_lookup : metadata->memberHybrids )
    {
        output << "### " << hybrid_lookup.name << " `= " << hybrid_lookup.valueDefault << "f` _- Hybrid Lookup_\n" << FormatDescription( hybrid_lookup.description ) << '\n';
    }

    for( auto& variable : metadata->memberVariables )
    {
        switch( variable.type )
        {
        case FastNoise::Metadata::MemberVariable::EFloat:
            output << "### " << FastNoise::Metadata::FormatMetadataMemberName( variable ) << " `= " << variable.valueDefault.f << "f`\n" << FormatDescription( variable.description ) << '\n';
            break;
        case FastNoise::Metadata::MemberVariable::EInt:
            output << "### " << FastNoise::Metadata::FormatMetadataMemberName( variable ) << " `= " << variable.valueDefault.i << "`\n" << FormatDescription( variable.description ) << '\n';
            break;
        case FastNoise::Metadata::MemberVariable::EEnum:
            output << "### " << FastNoise::Metadata::FormatMetadataMemberName( variable ) << " `= " << variable.enumNames[variable.valueDefault.i] << "` _- Enum_\n" << FormatDescription( variable.description ) << '\n';
            for( size_t i = 0; i < variable.enumNames.size(); i++ )
            {
                output << "* " << variable.enumNames[i] << (variable.valueDefault.i == i ? " (Default)\n" : "\n");
            }
            break;
        }
    }

}

int main( int argc, char* argv[] )
{
    std::string outputDir = ".";
    if( argc > 1 )
    {
        outputDir = argv[1];
        std::filesystem::create_directories( outputDir );
    }

    std::filesystem::create_directories( outputDir + "/images" );

    std::unordered_map<std::string, std::stringstream> outputStreams;

    for( const FastNoise::Metadata* metadata : FastNoise::Metadata::GetAll() )
    {
        const char* groupName = metadata->groups[0];

        if( outputStreams.try_emplace( groupName ).second )
        {
            outputStreams[groupName] << "# " << groupName << '\n';
            outputStreams[groupName].setf(std::ios::fixed);
            outputStreams[groupName].precision(1);
        }

        DoNode( outputStreams[groupName], metadata, outputDir );

    }

    for( auto& stream : outputStreams )
    {
        std::string fileName = stream.first;
        std::replace( fileName.begin(), fileName.end(), ' ', '-' );

        std::ofstream outFile( outputDir + "/Nodes#-" + fileName + ".md" );

        outFile << stream.second.str();
        outFile.close();

        std::cout << "Written " << fileName << ".md\n";
    }
}