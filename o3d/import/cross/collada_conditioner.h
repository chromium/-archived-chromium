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


// This file contains the declarations for the functions in the
// O3D COLLADA conditioner namespace.  These functions do most of
// the actual work of conditioning and packagaging a DAE file for use
// in O3D.

#ifndef O3D_IMPORT_CROSS_COLLADA_CONDITIONER_H_
#define O3D_IMPORT_CROSS_COLLADA_CONDITIONER_H_

#include "core/cross/types.h"
#include "compiler/technique/technique_structures.h"

class FCDEffect;
class FCDEffectProfileFX;
class FCDocument;
class FUUri;
class FilePath;

namespace o3d {

class ColladaZipArchive;
class ServiceLocator;

class ColladaConditioner {
 public:
  explicit ColladaConditioner(ServiceLocator* service_locator);
  ~ColladaConditioner();

  // This function conditions the given document for use in O3D.  This
  // mainly includes checking that the referenced shaders compile
  // against both the Cg (all platforms) and D3D (windows only)
  // runtimes, and converting the shaders to the common shader
  // language used by O3D.  The converts all the shaders to inline
  // "code" shaders instead of "included" shaders.  It also handles
  // converting "NV_Import" shaders produced by 3dsMax to something
  // o3d can read.  Returns false on failure.  If archive is non-NULL,
  // then it attempts to read shader files from the give zip archive
  // instead of from the disk.
  bool ConditionDocument(FCDocument* doc, ColladaZipArchive* archive);

  // This takes the given shader file and rewrites it to have the
  // proper form for O3D.  If the archive parameter is non-NULL, then
  // the input file is read from the archive instead of from the
  // regular filesystem.
  bool RewriteShaderFile(ColladaZipArchive* archive,
                         const FilePath& in_filename,
                         const FilePath& out_filename,
                         SamplerStateList* sampler_list,
                         String* vs_entry,
                         String* ps_entry);

  // This test-compiles the given shader source with the HLSL
  // compiler, if there is one available on the platform.  This
  // function is implemented separately for each platform.  If there
  // is no HLSL compiler available on this platform, this function
  // returns 'true'.
  bool CompileHLSL(const String& shader_source,
                   const String& vs_entry,
                   const String& ps_entry);

  // This test-compiles the given shader source with the Cg compiler,
  // if there is one available on the platform.  This function is
  // implemented separately for each platform.  If there is no Cg
  // compiler available on this platform, this function returns
  // 'true'.
  bool CompileCg(const FilePath& filename,
                 const String& shader_source,
                 const String& vs_entry,
                 const String& ps_entry);

 protected:
  // This preprocesses the given file using the Cgc compiler.  It
  // doesn't compile the shader, it just preprocesses it.  This is
  // implemented separately for each platform, since it invokes the
  // compiler as a separate process.
  bool PreprocessShaderFile(const FilePath& in_filename,
                            const FilePath& out_filename);

  // This takes the given COLLADA shader source code and rewrites it
  // to have the proper form for O3D.  It finds the entry points and
  // sets vs_entry and ps_entry to what it finds.
  bool RewriteShader(const String& shader_source_in,
                     String* shader_source_out,
                     const FilePath& in_filename,
                     SamplerStateList* sampler_list,
                     String* vs_entry,
                     String* ps_entry);

  // Handle all the embedded shaders in the document.
  bool HandleEmbeddedShaders(FCDEffect* collada_effect,
                             FCDEffectProfileFX* profile_fx,
                             ColladaZipArchive* archive);

  bool HandleNVImport(FCDocument* doc,
                      FCDEffect* collada_effect,
                      const FUUri& original_uri,
                      ColladaZipArchive* archive);
 private:
  ServiceLocator* service_locator_;

  DISALLOW_COPY_AND_ASSIGN(ColladaConditioner);
};
}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_COLLADA_CONDITIONER_H_
