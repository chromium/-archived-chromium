/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Driver function that sets up the Parser and Lexer for the "Technique"
// grammar and executes the parsing process, returning the results as a
// string and a Technique structure.
//
// Note that the "TechniqueParser.[c|h]" and "TechniqueLexer.[c|h]" files
// are generated in standard C and compiled under C++, but the code
// generator declares all functions as having "extern C" linkage. This may
// lead to compiler warnings about returning "struct" type values from a
// function - valid in C++, invalid under C.

#include "base/logging.h"
#include "compiler/technique/technique_parser.h"

namespace o3d {

// functions -------------------------------------------------------------------

// Use the Technique grammar to parse a UTF8 text file from the filesystem.
bool ParseFxFile(
    const String& filename,
    String *shader_string,
    SamplerStateList *sampler_list,
    TechniqueDeclarationList *technique_list,
    String *error_string) {
  // Create the input stream object from a file of UTF8 held in the filesystem.
  // The antlr3 library requires a non-const pointer to the filename.
  char* temp = const_cast<char*>(filename.c_str());
  pANTLR3_INPUT_STREAM input_stream = antlr3AsciiFileStreamNew(
      reinterpret_cast<pANTLR3_UINT8>(temp));
  if (input_stream == NULL) {
    DLOG(ERROR) << "Technique: Unable to open file" << filename
                << "due to memory allocation failure.";
    return false;
  } else {
    DLOG(INFO) << "Technique: Opened file \"" << filename << "\"";
  }

  // Create the language-specific Lexer.
  pTechniqueLexer lexer = TechniqueLexerNew(input_stream);
  if (lexer == NULL) {
    DLOG(ERROR) <<  "Technique: Unable to create the lexer";
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created lexer.";
  }

  // Create token stream from the Lexer.
  pANTLR3_COMMON_TOKEN_STREAM token_stream =
      antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
  if (token_stream == NULL)  {
    DLOG(ERROR) << "Technique: failed to allocate token stream.";
    lexer->free(lexer);
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created token stream.";
  }

  // Force the token stream to turn off token filtering and pass all
  // whitespace and comments through to the parser.
  token_stream->discardOffChannel = ANTLR3_FALSE;

  // Create the language Parser.
  pTechniqueParser parser = TechniqueParserNew(token_stream);
  if (parser == NULL) {
    DLOG(ERROR) << "Technique: Out of memory trying to allocate parser";
    token_stream->free(token_stream);
    lexer->free(lexer);
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created parser.";
  }

  // call the root parsing rule to parse the input stream.
  DLOG(INFO) << "Technique: Parsing...";
  shader_string->clear();
  technique_list->clear();
  sampler_list->clear();
  bool return_value  = parser->translation_unit(parser,
                                                technique_list,
                                                sampler_list,
                                                shader_string,
                                                error_string);
  DLOG(INFO) << "Technique: Finished parsing.";

  DLOG(INFO) << "Technique: Shader string = ";
  DLOG(INFO) << *shader_string;

  // Free created objects.
  parser->free(parser);
  token_stream->free(token_stream);
  lexer->free(lexer);
  input_stream->close(input_stream);

  // Finished
  return return_value;
}


// Use the Technique grammar to parse an FX file from an in-memory,
// zero-terminted UTF8 string buffer.
bool ParseFxString(
    const String& fx_string,
    String *shader_string,
    SamplerStateList *sampler_list,
    TechniqueDeclarationList *technique_list,
    String* error_string) {
  // Create a token stream from a zero-terminated memory buffer of uint8.
  ANTLR3_UINT32 fx_string_length =
      static_cast<ANTLR3_UINT32>(fx_string.length());
  if (fx_string_length == 0) {
    DLOG(INFO) << "Technique: fx string has zero length. Skipping.";
    return true;
  }

  // Create the input stream from the memory buffer.
  // The antlr3 stream library requires a non-const pointer to the fx_string.
  char* temp_fx_string = const_cast<char*>(fx_string.c_str());
  pANTLR3_INPUT_STREAM input_stream =
      antlr3NewAsciiStringInPlaceStream(
          reinterpret_cast<pANTLR3_UINT8>(temp_fx_string),
          fx_string_length,
          NULL);
  if (input_stream == NULL) {
    DLOG(ERROR) << "Technique: Unable to create input stream from string.";
    return false;
  } else {
    DLOG(INFO) << "Technique: Created input stream from string.";
  }

  // Create the language-specific Lexer.
  pTechniqueLexer lexer = TechniqueLexerNew(input_stream);
  if (lexer == NULL) {
    DLOG(ERROR) <<  "Technique: Unable to create the lexer";
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created lexer.";
  }

  // Create token stream from the Lexer.
  pANTLR3_COMMON_TOKEN_STREAM token_stream =
      antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
  if (token_stream == NULL)  {
    DLOG(ERROR) << "Technique: failed to allocate token stream.";
    lexer->free(lexer);
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created token stream.";
  }

  // Force the token stream to turn off token filtering and pass all
  // whitespace and comments through to the parser.
  token_stream->discardOffChannel = ANTLR3_FALSE;

  // Create the language Parser.
  pTechniqueParser parser = TechniqueParserNew(token_stream);
  if (parser == NULL) {
    DLOG(ERROR) << "Technique: Out of memory trying to allocate parser";
    token_stream->free(token_stream);
    lexer->free(lexer);
    input_stream->close(input_stream);
    return false;
  } else {
    DLOG(INFO) << "Technique: Created parser.";
  }

  // call the root parsing rule to parse the input stream.
  DLOG(INFO) << "Technique: Parsing...";
  shader_string->clear();
  technique_list->clear();
  sampler_list->clear();
  bool return_value = parser->translation_unit(parser,
                                               technique_list,
                                               sampler_list,
                                               shader_string,
                                               error_string);
  DLOG(INFO) << "Technique: Finished parsing.";

  DLOG(INFO) << "Technique: Shader string = ";
  DLOG(INFO) << *shader_string;

  // Free created objects.
  parser->free(parser);
  token_stream->free(token_stream);
  lexer->free(lexer);
  input_stream->close(input_stream);

  // Completed
  return return_value;
}

}  // namespace o3d
