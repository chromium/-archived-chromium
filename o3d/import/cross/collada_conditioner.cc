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

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "compiler/technique/technique_parser.h"
#include "core/cross/error.h"
#include "core/cross/types.h"
#include "import/cross/collada.h"
#include "import/cross/collada_zip_archive.h"
#include "utils/cross/file_path_utils.h"
#include "utils/cross/temporary_file.h"

namespace o3d {
namespace {
FUDaeTextureFilterFunction::FilterFunction LookupFilterFunction(
    const char* name) {
  struct {
    const char* name;
    FUDaeTextureFilterFunction::FilterFunction func;
  } functions[] = {
    "None", FUDaeTextureFilterFunction::NONE,
    "Linear", FUDaeTextureFilterFunction::LINEAR,
    "Point", FUDaeTextureFilterFunction::NEAREST,  // DX
    "Nearest", FUDaeTextureFilterFunction::NEAREST,  // GL
    "LinearMipmapLinear", FUDaeTextureFilterFunction::LINEAR_MIPMAP_LINEAR,
    "LinearMipmapNearest", FUDaeTextureFilterFunction::LINEAR_MIPMAP_NEAREST,
    "NearestMipmapNearest", FUDaeTextureFilterFunction::LINEAR_MIPMAP_NEAREST,
    "NearestMipmapLinear", FUDaeTextureFilterFunction::NEAREST_MIPMAP_LINEAR,
    // TODO: Once FCollada supports the COLLADA v1.5 spec,
    // turn this on.
    // "Anisotropic", FUDaeTextureFilterFunction::ANISOTROPIC,
  };
  for (int i = 0; i < sizeof(functions) / sizeof(functions[0]); ++i) {
    if (!base::strcasecmp(functions[i].name, name)) {
      return functions[i].func;
    }
  }
  return FUDaeTextureFilterFunction::UNKNOWN;
}

#undef CLAMP

FUDaeTextureWrapMode::WrapMode LookupWrapMode(const char* name) {
  struct {
    const char* name;
    FUDaeTextureWrapMode::WrapMode mode;
  } modes[] = {
    "None", FUDaeTextureWrapMode::NONE,
    // DX-style names:
    "Wrap", FUDaeTextureWrapMode::WRAP,
    "Mirror", FUDaeTextureWrapMode::MIRROR,
    "Clamp", FUDaeTextureWrapMode::CLAMP,
    "Border", FUDaeTextureWrapMode::BORDER,
    // GL-style names:
    "Repeat", FUDaeTextureWrapMode::WRAP,
    "MirroredRepeat", FUDaeTextureWrapMode::MIRROR,
    "ClampToEdge", FUDaeTextureWrapMode::CLAMP,
    "ClampToBorder", FUDaeTextureWrapMode::BORDER,
  };
  for (int i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
    if (!base::strcasecmp(modes[i].name, name)) {
      return modes[i].mode;
    }
  }
  return FUDaeTextureWrapMode::UNKNOWN;
}

FCDEffectParameter* FindParameter(FCDMaterial* material, const char* name) {
  for (size_t i = 0; i < material->GetEffectParameterCount(); ++i) {
    FCDEffectParameter* p = material->GetEffectParameter(i);
    if (!strcmp(p->GetReference(), name)) {
      return p;
    }
  }
  return NULL;
}

void SetSamplerStates(const SamplerState& sampler,
                      FCDEffectParameterSampler* sampler_out) {
  sampler_out->SetReference(sampler.name.c_str());
  sampler_out->SetMinFilter(LookupFilterFunction(sampler.min_filter.c_str()));
  sampler_out->SetMagFilter(LookupFilterFunction(sampler.mag_filter.c_str()));
  sampler_out->SetMipFilter(LookupFilterFunction(sampler.mip_filter.c_str()));
  sampler_out->SetWrapS(LookupWrapMode(sampler.address_u.c_str()));
  sampler_out->SetWrapT(LookupWrapMode(sampler.address_u.c_str()));

  // TODO: Once FCollada supports the COLLADA v1.5 spec, turn this on.
  // sampler_out->SetMaxAnisotropy(atoi(sampler.max_anisotropy.c_str()));
}

const PassDeclaration* FindValidTechnique(
    const TechniqueDeclarationList& technique_list) {

  // Look for a ps2_0/vs2_0 technique, or an arbfp1/arbvp1 technique
  TechniqueDeclarationList::const_iterator i;
  for (i = technique_list.begin(); i != technique_list.end(); ++i) {
    // Skip all multi-pass techniques
    if (i->pass.size() != 1) continue;

    const PassDeclaration& pass = i->pass[0];
    if (pass.vertex_shader_profile == "vs_2_0" &&
        pass.fragment_shader_profile == "ps_2_0" ||
        pass.vertex_shader_profile == "arbvp1" &&
        pass.fragment_shader_profile == "arbfp1") {
      return &pass;
    }
  }
  return NULL;
}

bool IsColumnMajor(const PassDeclaration* pass) {
  return pass->vertex_shader_profile == "arbvp1" ||
      pass->fragment_shader_profile == "arbfp1";
}
}  // end anonymous namespace.

ColladaConditioner::ColladaConditioner(ServiceLocator* service_locator)
    : service_locator_(service_locator) {
}

ColladaConditioner::~ColladaConditioner() {
}

bool ColladaConditioner::HandleEmbeddedShaders(FCDEffect* collada_effect,
                                               FCDEffectProfileFX* profile_fx,
                                               ColladaZipArchive* archive) {
  bool found = false;
  size_t len = profile_fx->GetTechniqueCount();
  for (size_t j = 0; j < len && !found; ++j) {
    FCDEffectTechnique* technique = profile_fx->GetTechnique(j);
    if (!technique) continue;
    // We only support single-pass effects (for now).
    if (technique->GetPassCount() != 1) continue;
    FCDEffectPass* pass = technique->GetPass(0);
    if (!pass) continue;
    FCDEffectPassShader* vertex_shader = pass->GetVertexShader();
    if (!vertex_shader) continue;
    FCDEffectPassShader* fragment_shader = pass->GetFragmentShader();
    if (!fragment_shader) continue;
    // Note:  We ignore the compiler targets in the ColladaFX section,
    // since they are often wrong (ColladaMAX puts ps 3.0/vs 3.0 in,
    // regardless of the actual shader), and consult the shader file
    // itself instead.
    SamplerStateList sampler_list;
    FCDEffectCode* code = vertex_shader->GetCode();
    String shader_source_out;
    if (code->GetType() == FCDEffectCode::CODE) {
      fstring code_string = code->GetCode();
      String shader_source_in(WideToUTF8(code_string.c_str()));
      FilePath stdin_path(FILE_PATH_LITERAL("<stdin>"));
      if (RewriteShader(shader_source_in,
                        &shader_source_out,
                        stdin_path,
                        &sampler_list,
                        NULL,
                        NULL)) {
        found = true;
      }
      code->SetCode(UTF8ToWide(shader_source_out).c_str());
    } else if (code->GetType() == FCDEffectCode::INCLUDE) {
      FilePath file_path(WideToFilePath(code->GetFilename().c_str()));
      TemporaryFile temp_file;
      if (!TemporaryFile::Create(&temp_file)) {
        O3D_ERROR(service_locator_) << "Unable to create temporary file '"
                                    << FilePathToUTF8(temp_file.path()).c_str()
                                    << "' for rewriting shader.";
        return false;
      }
      if (!RewriteShaderFile(archive,
                             file_path,
                             temp_file.path(),
                             &sampler_list,
                             NULL,
                             NULL)) {
        return false;
      }

      // Read the shader from the temp file, and add it to the output map.
      String shader_source;
      if (!file_util::ReadFileToString(temp_file.path(), &shader_source)) {
        O3D_ERROR(service_locator_) << "Unable to read temporary file.";
        return false;
      }
      code->SetCode(UTF8ToWide(shader_source).c_str());
    }
  }

  if (!found) {
    String effect_name = WideToUTF8(collada_effect->GetName().c_str());
    O3D_ERROR(service_locator_) << "No valid technique found for effect \""
                                << effect_name << "\".";
    return false;
  }
  return true;
}

// This non-standard effect technique (NV_import) is the one used by
// 3ds Max when exporting files using native DX materials, so
// unfortunately we have to support it.
bool ColladaConditioner::HandleNVImport(FCDocument* doc,
                                        FCDEffect* collada_effect,
                                        const FUUri& original_uri,
                                        ColladaZipArchive* archive) {
  FCDExtra* extra = collada_effect->GetExtra();
  // There is no actual type tag in the XML, but FCollada constructs one
  // for us anyway.
  if (extra && extra->GetTypeCount() > 0) {
    FCDEType* type = extra->GetType(0);
    if (type) {
      FCDETechnique* technique = type->FindTechnique("NV_import");
      if (technique) {
        FCDENode* node = technique->FindChildNode("import");
        if (node) {
          FCDEAttribute* url_attrib = node->FindAttribute("url");
          if (url_attrib) {
            FUFileManager* mgr = doc->GetFileManager();
            DCHECK(mgr != NULL);
            // Escape any %hex values in the URL, resolve it relative
            // to the document root, and convert it to an absolute path.
            fstring url = FUXmlParser::XmlToString(url_attrib->GetValue());
            FUUri effect_uri = original_uri.Resolve(url);
            fstring path = effect_uri.GetAbsolutePath();

            FilePath in_filename(WideToFilePath(path.c_str()));
            TemporaryFile temp_file;
            if (!TemporaryFile::Create(&temp_file)) {
              O3D_ERROR(service_locator_) << "Unable to create temporary file.";
            }
            SamplerStateList sampler_list;
            // Check that the file exists; error if not.
            if (!mgr->FileExists(path)) {
              O3D_ERROR(service_locator_) << "Shader file \""
                                          << WideToUTF8(path.c_str()).c_str()
                                          << "\" does not exist.";
              return false;
            }

            String vs_entry;
            String ps_entry;
            if (!RewriteShaderFile(archive,
                                   in_filename,
                                   temp_file.path(),
                                   &sampler_list,
                                   &vs_entry,
                                   &ps_entry)) {
              return false;
            }

            // Create a new HLSL profile to hold the rewritten effect.
            FCDEffectProfile* profile =
                collada_effect->AddProfile(FUDaeProfileType::HLSL);
            FCDEffectProfileFX* profile_fx =
                down_cast<FCDEffectProfileFX*>(profile);
            // Move the shader file to the COLLADA-FX section.
            FCDEffectTechnique* fx_technique = profile_fx->AddTechnique();

            FCDEffectCode* code = profile_fx->AddCode();

            // Read the shader from the temp file, and add it to the
            // output map.
            String shader_source;
            if (!file_util::ReadFileToString(temp_file.path(),
                                             &shader_source)) {
              O3D_ERROR(service_locator_) << "Unable to read temporary file.";
              return false;
            }

            // Set the embedded code to the rewritten shader.
            code->SetCode(UTF8ToWide(shader_source).c_str());

            FCDEffectPass* pass = fx_technique->AddPass();
            FCDEffectPassShader* vertex_shader = pass->AddVertexShader();
            FCDEffectPassShader* fragment_shader =
                pass->AddFragmentShader();

            vertex_shader->SetCode(code);
            vertex_shader->SetName(vs_entry.c_str());

            fragment_shader->SetCode(code);
            fragment_shader->SetName(ps_entry.c_str());

            // Change the setparams
            SamplerStateList::const_iterator it;
            for (it = sampler_list.begin(); it != sampler_list.end(); ++it) {
              // Create a <sampler> tag.
              FCDEffectParameterSampler* sampler =
                  down_cast<FCDEffectParameterSampler*>(
                      profile->AddEffectParameter(
                          FCDEffectParameter::SAMPLER));
              LOG_ASSERT(sampler != 0);
              // Set the sampler parameters from the ones found in
              // the FX file.
              SetSamplerStates(*it, sampler);
            }

            // For each material which uses this effect,
            FCDMaterialLibrary* lib = doc->GetMaterialLibrary();
            for (size_t i = 0; i < lib->GetEntityCount(); ++i) {
              FCDMaterial* material = lib->GetEntity(i);
              LOG_ASSERT(material != 0);
              if (material->GetEffect() == collada_effect) {
                // For each sampler in the FX file,
                SamplerStateList::const_iterator it;
                for (it = sampler_list.begin();
                     it != sampler_list.end();
                     ++it) {
                  // Create a <sampler> tag.
                  FCDEffectParameterSampler* sampler =
                      down_cast<FCDEffectParameterSampler*>(
                          material->AddEffectParameter(
                              FCDEffectParameter::SAMPLER));
                  LOG_ASSERT(sampler != 0);
                  // Set the sampler states from FX file states
                  SetSamplerStates(*it, sampler);
                  // Set this sampler to be a modifier, so it appears
                  // as a <setparam> tag in the COLLADA file.
                  sampler->SetModifier();
                  // Set its surface to be the mapping from the texture
                  // attribute of the sampler_state in the FX file.
                  FCDEffectParameter* surface = FindParameter(
                      material, it->texture.c_str());
                  if (surface && surface->GetType() ==
                      FCDEffectParameter::SURFACE) {
                    sampler->SetSurface(
                        down_cast<FCDEffectParameterSurface*>(surface));
                  }
                }
              }
            }

            // Remove the NV_import technique from the COLLADA DOM.
            technique->Release();
          }
        }
      }
    }
  }
  return true;
}

// Verifies that all shaders conform to O3D's shader language
// (intersection of Cg and HLSL), and fix the shader and image file URLs
// to be relative to the output file root.
bool ColladaConditioner::ConditionDocument(FCDocument* doc,
                                           ColladaZipArchive* archive) {
  FUUri original_uri = doc->GetFileUrl();
  FCDEffectLibrary* effectLibrary = doc->GetEffectLibrary();

  for (size_t i = 0; i < effectLibrary->GetEntityCount(); ++i) {
    FCDEffect* collada_effect = effectLibrary->GetEntity(i);
    DCHECK(collada_effect != NULL);
    FCDEffectProfile* profile =
        collada_effect->FindProfile(FUDaeProfileType::HLSL);
    if (!profile) {
      profile = collada_effect->FindProfile(FUDaeProfileType::CG);
    }
    if (profile) {
      // Handle embedded shaders.
      FCDEffectProfileFX* profile_fx =
          static_cast<FCDEffectProfileFX*>(profile);
      if (!HandleEmbeddedShaders(collada_effect, profile_fx, archive)) {
        return false;
      }
    } else {
      if (!HandleNVImport(doc, collada_effect, original_uri, archive)) {
        return false;
      }
    }
  }
  return true;
}

// Rewrites the given shader file to conform to o3d specs.  This
// function finds a valid ps2.0/vs2.0 or arbvp/fp technique, writes
// out the entry points in our comment format, and writes out the
// shader without technique blocks.  Returns false if in_filename
// can't be opened for reading, out_filename can't be opened for
// writing, or if the shader is not valid (see RewriteShader).  If the
// archive is non-NULL, reads its input from the given zip archive
// instead of from the file system.
bool ColladaConditioner::RewriteShaderFile(ColladaZipArchive* archive,
                                           const FilePath& in_filename,
                                           const FilePath& out_filename,
                                           SamplerStateList* sampler_list,
                                           String* vs_entry,
                                           String* ps_entry) {
  FilePath input_file = in_filename;
  TemporaryFile temporary_output_file;
  TemporaryFile temporary_input_file;
  if (!TemporaryFile::Create(&temporary_output_file)) {
    O3D_ERROR(service_locator_) << "Unable to create temporary file.";
    return false;
  }
  if (archive) {
    if (!TemporaryFile::Create(&temporary_input_file)) {
      O3D_ERROR(service_locator_) << "Unable to create temporary file.";
      return false;
    }
    input_file = temporary_input_file.path();
    size_t size = 0;
    char* contents = archive->GetColladaAssetData(FilePathToUTF8(in_filename),
                                                  &size);
    if (file_util::WriteFile(input_file, contents, size) == -1) {
      O3D_ERROR(service_locator_) << "Unable to write to temporary file.";
      return false;
    }
  }
  if (!PreprocessShaderFile(input_file, temporary_output_file.path())) {
    return false;
  }
  String shader_source_in;
  if (!file_util::ReadFileToString(temporary_output_file.path(),
                                   &shader_source_in)) {
    O3D_ERROR(service_locator_) << "Unable to read temporary file.";
    return false;
  }

  String shader_source_out;
  if (!RewriteShader(shader_source_in,
                     &shader_source_out,
                     in_filename,
                     sampler_list,
                     vs_entry,
                     ps_entry)) {
    return false;
  }

  if (file_util::WriteFile(out_filename,
                           shader_source_out.c_str(),
                           shader_source_out.size())  == -1) {
    O3D_ERROR(service_locator_) << "Couldn't write temporary shader file "
                                << FilePathToUTF8(out_filename).c_str();
    return false;
  }
  return true;
}

// Rewrites the given shader to conform to o3d specs.
// This function finds a valid ps2.0/vs2.0 or arbvp/fp technique,
// strips the technique block from the input shader, and adds the
// vertex and fragment shader entry points in our comment format.
// Returns false if no valid technique could be found, or if the
// resulting shader cannot not be compiled as both HLSL and Cg.
bool ColladaConditioner::RewriteShader(const String& shader_source_in,
                                       String* shader_source_out,
                                       const FilePath& in_filename,
                                       SamplerStateList* sampler_list,
                                       String* ps_entry,
                                       String* vs_entry) {
  TechniqueDeclarationList technique_list;
  bool column_major;

  // Parse out the technique block and samplers from the file
  String error_string;
  if (!ParseFxString(shader_source_in,
                     shader_source_out,
                     sampler_list,
                     &technique_list,
                     &error_string)) {
    O3D_ERROR(service_locator_) << error_string;
    return false;
  }

  const PassDeclaration* pass = FindValidTechnique(technique_list);

  if (!pass) {
    O3D_ERROR(service_locator_)
        << "Couldn't find compatible technique in effect file \""
        << FilePathToUTF8(in_filename).c_str() << "\".";
    return false;
  }

  if (!CompileHLSL(shader_source_out->c_str(),
                   pass->vertex_shader_entry,
                   pass->fragment_shader_entry)) {
    O3D_ERROR(service_locator_) << "Shader file \""
                                << FilePathToUTF8(in_filename).c_str()
                                << "\" could not be compiled as HLSL.\n";
    return false;
  }

  if (!CompileCg(in_filename,
                 shader_source_out->c_str(),
                 pass->vertex_shader_entry,
                 pass->fragment_shader_entry)) {
    O3D_ERROR(service_locator_) << "Shader file \""
                                << FilePathToUTF8(in_filename).c_str()
                                << "\" could not be compiled as Cg.\n";
    return false;
  }

  if (vs_entry) *vs_entry = pass->vertex_shader_entry;
  if (ps_entry) *ps_entry = pass->fragment_shader_entry;
  column_major = IsColumnMajor(pass);
  *shader_source_out += "// #o3d VertexShaderEntryPoint ";
  *shader_source_out += pass->vertex_shader_entry + "\n";
  *shader_source_out += "// #o3d PixelShaderEntryPoint ";
  *shader_source_out += pass->fragment_shader_entry + "\n";
  *shader_source_out += "// #o3d MatrixLoadOrder ";
  *shader_source_out += column_major ? "ColumnMajor" : "RowMajor";
  *shader_source_out += "\n";

  return true;
}

bool ColladaConditioner::CompileCg(const FilePath& filename,
                                   const String& shader_source,
                                   const String& vs_entry,
                                   const String& ps_entry) {
  bool retval = false;
  String shader_source_cg = shader_source;
  shader_source_cg +=
      "technique t {\n"
      "  pass p {\n"
      "    VertexShader = compile arbvp1 " + vs_entry + "();\n"
      "    PixelShader = compile arbfp1 " + ps_entry + "();\n"
      "  }\n"
      "};\n";

  // Create a Cg context in which to compile the given .FX file.
  CGcontext context = cgCreateContext();

  cgGLRegisterStates(context);

  // Create a Cg effect from the FX file.
  CGeffect effect = cgCreateEffect(context, shader_source_cg.c_str(), NULL);
  if (!effect || !cgIsEffect(effect) || cgGetError() != CG_NO_ERROR) {
    const char* errors = cgGetLastListing(context);
    String message = FilePathToUTF8(filename) + ":\n";
    if (errors) {
      message += String(errors) + "\n";
    } else {
      message += "Unknown Cg compilation error.\n";
    }
    O3D_ERROR(service_locator_) << message;
  } else {
    cgDestroyEffect(effect);
    retval = true;
  }
  cgDestroyContext(context);
  return retval;
}
}  // namespace o3d
