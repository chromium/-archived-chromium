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
// Parse an HLSL or Cg ".fx" file and break it into:
//     a) source code for the shader
//     b) structured information from a "technique{}" block, if one exists.
//
// Entry functions initialize the Parser and Lexer for the "Technique"
// grammar and execute the parsing process, returning the results as a
// string and a Technique object.
//
// Note that the two headers "TechniqueLexer.h" and "TechniqueParser.h" are
// automatically generated using the Antlr compiler-generator from the
// grammar file "Technique.g3pl"
//
// TODO: Note that this is a specialized grammar designed for this
// purpose alone, and it does no sanity checking on the HLSL source
// structure or identifiers itself. A more complete HLSL grammar and
// analyser for this task can be found in the directory
// "o3d/compiler/hlsl".

#ifndef O3D_COMPILER_TECHNIQUE_TECHNIQUE_PARSER_H_
#define O3D_COMPILER_TECHNIQUE_TECHNIQUE_PARSER_H_

#include <build/build_config.h>
#include <vector>
#include "core/cross/types.h"
#include "compiler/technique/technique_structures.h"
#include "compiler/technique/technique_error.h"

#ifdef OS_WIN
// NOTE: disable compiler warning about C function returning a
// struct caused by the Antlr generated functions being declared extern"C"
// but being compiled and used as C++.
#pragma warning(disable: 4190)
#endif

// As these headers are automatically generated, their full path has to be
// passed via the build script as each build target will generate the
// headers in a different location.
#include "TechniqueLexer.h"
#include "TechniqueParser.h"

namespace o3d {

// typedefs --------------------------------------------------------------------

typedef std::vector<TechniqueDeclaration> TechniqueDeclarationList;


// functions -------------------------------------------------------------------


// Use the Technique grammar to parse a UTF8 text file from the filing system.
// Inputs:
//    filename       - a UTF8 filename to send to the filesystem.
//    shader_string  - pointer to a String object that will be cleared.
//    sampler_list   - pointer to a vector of SamplerState objects.
//    technique_list - pointer to a vector of TechniqueDeclaration objects.
//    error_string   - pointer to a String object that will be cleared.
// Returns:
//    shader_string  - pointer to a String object containing the shader source.
//    sampler_list   - pointer to a vector of SamplerState objects, each filled
//                     in with fields parsed from the FX file.
//    technique_list - pointer to a vector of TechniqueDeclaration objects, each
//                     filled with fields parsed from the FX file.
//    error_string   - pointer to a String object containing parse errors,
//                     if any.
bool ParseFxFile(
    const String &filename,
    String *shader_string,
    SamplerStateList *sampler_list,
    TechniqueDeclarationList *technique_list,
    String *error_string);


// Use the Technique grammar to parse an in-memory string buffer.
// Inputs:
//    fx_string      - a zero-terminated UTF8 string of the .fx file source.
//    shader_string  - pointer to a String object that will be cleared.
//    sampler_list   - pointer to a vector of SamplerState objects.
//    technique_list - pointer to a vector of TechniqueDeclaration objects.
//    error_string   - pointer to a String object that will be cleared.
// Returns:
//    shader_string  - pointer to a String object containing the shader source.
//    sampler_list   - pointer to a vector of SamplerState objects, each filled
//                     in with fields parsed from the FX file.
//    technique_list - pointer to a vector of TechniqueDeclaration objects, each
//                     filled with fields parsed from the FX file.
//    error_string   - pointer to a String object containing parse errors,
//                     if any.
bool ParseFxString(
    const String &fx_string,
    String *shader_string,
    SamplerStateList *sampler_list,
    TechniqueDeclarationList *technique_list,
    String *error_string);

}  // namespace o3d

#endif  // O3D_COMPILER_TECHNIQUE_TECHNIQUE_PARSER_H_
