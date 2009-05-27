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


#include "import/cross/precompile.h"

#include "import/cross/collada_conditioner.h"

#include "core/cross/service_locator.h"
#include "import/cross/collada.h"

namespace o3d {
ColladaConditioner::ColladaConditioner(ServiceLocator* service_locator)
    : service_locator_(service_locator) {
}

ColladaConditioner::~ColladaConditioner() {
}

bool ColladaConditioner::HandleEmbeddedShaders(FCDEffect* collada_effect,
                                               FCDEffectProfileFX* profile_fx,
                                               ColladaZipArchive* archive) {
  return true;
}

bool ColladaConditioner::HandleNVImport(FCDocument* doc,
                                        FCDEffect* collada_effect,
                                        const FUUri& original_uri,
                                        ColladaZipArchive* archive) {
  return true;
}

bool ColladaConditioner::ConditionDocument(FCDocument* doc,
                                           ColladaZipArchive* archive) {
  return true;
}

bool ColladaConditioner::RewriteShaderFile(ColladaZipArchive* archive,
                                           const FilePath& in_filename,
                                           const FilePath& out_filename,
                                           SamplerStateList* sampler_list,
                                           String* vs_entry,
                                           String* ps_entry) {
  return true;
}

bool ColladaConditioner::RewriteShader(const String& shader_source_in,
                                       String* shader_source_out,
                                       const FilePath& in_filename,
                                       SamplerStateList* sampler_list,
                                       String* ps_entry,
                                       String* vs_entry) {
  return true;
}

bool ColladaConditioner::CompileCg(const FilePath& filename,
                                   const String& shader_source,
                                   const String& vs_entry,
                                   const String& ps_entry) {
  return true;
}
}  // namespace o3d
