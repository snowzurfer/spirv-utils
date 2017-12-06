/*
  MIT License

  Copyright (c) 2017 Dan Ginsburg

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/
#include <spv_utils.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <assert.h>

// The following example demonstrates how to use spv_utils to patch a vertex shader
// to invert the built-in y position.  This was used to be able to use the same
// HLSL/SPIR-V on implementations not supporting VK_KHR_maintenance1 which allows
// viewport y inversion.  
// This code is in no way meant to be entirely robust and will certainly fail in
// the case where the last output to Position in the module is not truly the last
// one.  However, it did work with the set of shaders I tested with and perhaps it
// might be a useful example for others.
int main( int argc, char *argv[] )
{
    if ( argc != 3 )
    {
        printf( "Usage: %s input.spv output.spv", argv[ 0 ] );
        return 1;
    }


    // Read spv binary module from a file
    std::ifstream spvFile( argv[ 1 ], std::ios::binary | std::ios::ate | std::ios::in );
    if ( !spvFile.is_open() )
    {
        printf( "Error opening input SPIR-V: %s\n", argv[ 1 ] );
        return 1;
    }

    std::streampos size = spvFile.tellg();

    spvFile.seekg( 0, std::ios::beg );
    char *data = new char[ size ];
    spvFile.read( data, size );
    spvFile.close();

    // Parse the module
    sut::OpcodeStream stream( data, size );

    // First find the position output, float scalar type, and float vec4 type ids
    spv::Id nPositionId = 0;
    spv::Id nScalarFloatTypeId = 0;
    spv::Id nFloat4TypeId = 0;
    bool bFoundPosition = false;
    bool bFoundTypeFloat = false;
    bool bFoundTypeFloat4 = false;
    {
        sut::OpcodeStream::iterator it = stream.begin();
        while( it != stream.end() && ( !bFoundPosition || !bFoundTypeFloat || !bFoundTypeFloat4 ) )
        {
            // Looking for OpDecorate Position
            if ( !bFoundPosition && ( it->GetOpcode() == spv::Op::OpDecorate ) )
            {
                std::vector< uint32_t > &words = it->GetWords();
                spv::Id nId = ( spv::Id ) words[ it->offset() + 1 ];
                spv::Decoration nDecoration = ( spv::Decoration ) words[ it->offset() + 2 ];
                if ( nDecoration  == spv::Decoration::BuiltIn )
                {
                    for ( uint32_t nDecorationCount = 0; nDecorationCount < it->GetWordCount() - 3; nDecorationCount++ )
                    {
                        spv::BuiltIn nBuiltIn = ( spv::BuiltIn ) words[ it->offset() + 3 + nDecorationCount ];
                        if ( nBuiltIn == spv::BuiltIn::Position )
                        {
                            nPositionId = nId;
                            bFoundPosition = true;
                        }
                    }
                }
            }
            // OpTypeFloat 32
            else if ( !bFoundTypeFloat && ( it->GetOpcode() == spv::Op::OpTypeFloat ) )
            {
                std::vector< uint32_t > &words = it->GetWords();
                nScalarFloatTypeId = words[ it->offset() + 1 ];
                bFoundTypeFloat = true;
            }
            // OpTypeVector nScalartFloatTypeId 4
            else if ( !bFoundTypeFloat4 && ( it->GetOpcode() == spv::Op::OpTypeVector ) )
            {
                assert( bFoundTypeFloat );
                std::vector< uint32_t > &words = it->GetWords();
                spv::Id nVectorTypeId = ( spv::Id ) words[ it->offset() + 2 ];
                if ( nVectorTypeId == nScalarFloatTypeId && ( words[ it->offset() + 3 ] == 4 ) )
                {
                    nFloat4TypeId = words[ it->offset() + 1 ];
                    bFoundTypeFloat4 = true;
                }
            }
            it++;
        }
    }

    // Now find the last write to position, and prepend y inversion
    if ( bFoundTypeFloat && bFoundPosition && bFoundTypeFloat4 )
    {
        // The bound is stored at the 3rd word, we need to bump this to have room for three more IDs
        uint32_t nBound = stream.GetWordsStream()[ 3 ];

        // Start new IDs after the bound, the 
        spv::Id nYScalarId = nBound;
        spv::Id nYScalarNegId = nBound + 1;
        spv::Id nNewObjectId = nBound + 2;
        sut::OpcodeStream::reverse_iterator rit = stream.rbegin();
        while ( rit != stream.rend() )
        {
            // Find OpCodeStore to the Position, searching backwards from the last instruction
            if ( rit->GetOpcode() == spv::Op::OpStore )
            {
                std::vector< uint32_t > &words = rit->GetWords();
                spv::Id nStoreId = ( spv::Id ) words[ rit->offset() + 1 ];
                spv::Id nObjectId = ( spv::Id ) words[ rit->offset() + 2 ];

                // Found a store to position
                if ( nStoreId == nPositionId )
                {
                    sut::OpcodeHeader header;

                    // Extract the y from position
                    // nYScalarId = OpCompositeExtract %float %nObjectId 1
                    header.opcode = ( uint16_t ) spv::Op::OpCompositeExtract;
                    header.words_count = 5;
                    std::vector< uint32_t > compositeExtract;
                    compositeExtract.push_back( MergeSpvOpCode( header ) );
                    compositeExtract.push_back( nScalarFloatTypeId );
                    compositeExtract.push_back( nYScalarId );
                    compositeExtract.push_back( nObjectId );
                    compositeExtract.push_back( 1 );

                    // Negate y
                    // nYScalarNegId = OpFNegate %float nYScaleId
                    header.opcode = ( uint16_t ) spv::Op::OpFNegate;
                    header.words_count = 4;
                    std::vector< uint32_t > negate;
                    negate.clear();
                    negate.push_back( MergeSpvOpCode( header ) );
                    negate.push_back( nScalarFloatTypeId );
                    negate.push_back( nYScalarNegId );
                    negate.push_back( nYScalarId );

                    // Create a new vec4 that has inverted y, copying the rest of the object as is
                    // nNewObjectId = OpCompositeInsert %v4float %nYScalarNegId %nObjectId 1
                    header.opcode = ( uint16_t ) spv::Op::OpCompositeInsert;
                    header.words_count = 6;
                    std::vector< uint32_t > compositeInsert;
                    compositeInsert.push_back( MergeSpvOpCode( header ) );
                    compositeInsert.push_back( nFloat4TypeId );
                    compositeInsert.push_back( nNewObjectId );
                    compositeInsert.push_back( nYScalarNegId );
                    compositeInsert.push_back( nObjectId );
                    compositeInsert.push_back( 1 );

                    // Modify which id the OpStore is from
                    words[ rit->offset() + 2 ] = nNewObjectId;

                    // Also modify the bounds, which is always at the 3rd word in the SPIR-V.  We've added
                    // two new IDs
                    words[ 3 ] = nBound + 3;

                    // Finally, insert the instructions before the store.  These get inserted in reverse order
                    // because of how InsertBefore behaves
                    rit->InsertBefore( &compositeInsert[ 0 ], compositeInsert.size() );
                    rit->InsertBefore( &negate[ 0 ], negate.size() );
                    rit->InsertBefore( &compositeExtract[ 0 ], compositeExtract.size() );
                    break;
                }
            }
            rit++;
        }

        sut::OpcodeStream patchedStream = stream.EmitFilteredStream();
        std::vector< uint32_t > patchedStreamWords = patchedStream.GetWordsStream();

        std::ofstream spvOutputFile( argv[ 2 ], std::ios::binary | std::ios::out );
        if ( !spvOutputFile.is_open() )
        {
            printf( "Error opening output SPIR-V: %s\n", argv[ 2 ] );
            return 1;
        }
        spvOutputFile.write( ( const char * ) &patchedStreamWords[ 0 ], patchedStreamWords.size() * sizeof( uint32_t ) );
        spvOutputFile.close();
    }
    else
    {
        printf( "Shader was determined not to require patching, no output written.\n" );
    }

    delete[] data;
    return 0;
}
