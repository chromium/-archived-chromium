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


// This file contains functions for importing COLLADA files into O3D.
#include "import/cross/precompile.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "core/cross/curve.h"
#include "core/cross/error.h"
#include "core/cross/function.h"
#include "core/cross/ierror_status.h"
#include "core/cross/math_utilities.h"
#include "core/cross/matrix4_axis_rotation.h"
#include "core/cross/matrix4_composition.h"
#include "core/cross/matrix4_scale.h"
#include "core/cross/matrix4_translation.h"
#include "core/cross/pack.h"
#include "core/cross/param_operation.h"
#include "core/cross/primitive.h"
#include "core/cross/skin.h"
#include "core/cross/stream.h"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"
#include "import/cross/collada_zip_archive.h"
#include "utils/cross/file_path_utils.h"

#define COLLADA_NAMESPACE "collada"
#define COLLADA_NAMESPACE_SEPARATOR "."

// Macro to provide a uniform prefix for all string constants created by
// COLLADA.
#define COLLADA_STRING_CONSTANT(value)                  \
  (COLLADA_NAMESPACE COLLADA_NAMESPACE_SEPARATOR value)


namespace o3d {

const char* Collada::kLightingTypeParamName =
    COLLADA_STRING_CONSTANT("lightingType");

const char* Collada::kLightingTypeConstant = "constant";
const char* Collada::kLightingTypePhong = "phong";
const char* Collada::kLightingTypeBlinn = "blinn";
const char* Collada::kLightingTypeLambert = "lambert";
const char* Collada::kLightingTypeUnknown = "unknown";

const char* Collada::kMaterialParamNameEmissive = "emissive";
const char* Collada::kMaterialParamNameAmbient = "ambient";
const char* Collada::kMaterialParamNameDiffuse = "diffuse";
const char* Collada::kMaterialParamNameSpecular = "specular";
const char* Collada::kMaterialParamNameShininess = "shininess";
const char* Collada::kMaterialParamNameSpecularFactor = "specularFactor";
const char* Collada::kMaterialParamNameEmissiveSampler = "emissiveSampler";
const char* Collada::kMaterialParamNameAmbientSampler = "ambientSampler";
const char* Collada::kMaterialParamNameDiffuseSampler = "diffuseSampler";
const char* Collada::kMaterialParamNameSpecularSampler = "specularSampler";
const char* Collada::kMaterialParamNameBumpSampler = "bumpSampler";

class TranslationMap : public FCDGeometryIndexTranslationMap {
};

namespace {

Vector3 FMVector3ToVector3(const FMVector3& fmvector3) {
  return Vector3(fmvector3.x, fmvector3.y, fmvector3.z);
}

Vector4 FMVector4ToVector4(const FMVector4& fmvector4) {
  return Vector4(fmvector4.x, fmvector4.y, fmvector4.z, fmvector4.w);
}

Float2 FMVector2ToFloat2(const FMVector2& fmvector2) {
  return Float2(fmvector2.x, fmvector2.y);
}

Matrix4 FMMatrix44ToMatrix4(const FMMatrix44& fmmatrix44) {
  return Matrix4(Vector4(fmmatrix44[0][0],
                         fmmatrix44[0][1],
                         fmmatrix44[0][2],
                         fmmatrix44[0][3]),
                 Vector4(fmmatrix44[1][0],
                         fmmatrix44[1][1],
                         fmmatrix44[1][2],
                         fmmatrix44[1][3]),
                 Vector4(fmmatrix44[2][0],
                         fmmatrix44[2][1],
                         fmmatrix44[2][2],
                         fmmatrix44[2][3]),
                 Vector4(fmmatrix44[3][0],
                         fmmatrix44[3][1],
                         fmmatrix44[3][2],
                         fmmatrix44[3][3]));
}
}  // anonymous namespace

// Import the given COLLADA file or ZIP file into the given scene.
// This is the external interface to o3d.
bool Collada::Import(Pack* pack,
                     const FilePath& filename,
                     Transform* parent,
                     ParamFloat* animation_input,
                     const Options& options) {
  Collada collada(pack, options);
  return collada.ImportFile(filename, parent, animation_input);
}

bool Collada::Import(Pack* pack,
                     const String& filename,
                     Transform* parent,
                     ParamFloat* animation_input,
                     const Options& options) {
  return Collada::Import(pack,
                         UTF8ToFilePath(filename),
                         parent,
                         animation_input,
                         options);
}

// Parameters:
//   pack:      The pack into which the scene objects will be placed.
// Returns true on success.
Collada::Collada(Pack* pack, const Options& options)
    : service_locator_(pack->service_locator()),
      pack_(pack),
      options_(options),
      dummy_effect_(NULL),
      dummy_material_(NULL),
      instance_root_(NULL),
      cull_enabled_(false),
      cull_front_(false),
      front_cw_(false),
      collada_zip_archive_(NULL),
      unique_filename_counter_(0) {
}

Collada::~Collada() {
  delete collada_zip_archive_;
}

void Collada::ClearData() {
  textures_.clear();
  original_data_.clear();
  effects_.clear();
  shapes_.clear();
  skinned_shapes_.clear();
  materials_.clear();
  delete collada_zip_archive_;
  collada_zip_archive_ = NULL;
  cull_enabled_ = false;
  cull_front_ = false;
  front_cw_ = false;
  delete instance_root_;
  instance_root_ = NULL;
  base_path_ = FilePath(FilePath::kCurrentDirectory);
  unique_filename_counter_ = 0;
}

std::vector<FilePath> Collada::GetOriginalDataFilenames() const {
  std::vector<FilePath> result;
  for (OriginalDataMap::const_iterator iter = original_data_.begin();
       iter != original_data_.end();
       ++iter) {
    result.push_back(iter->first);
  }
  return result;
}

const std::string& Collada::GetOriginalData(const FilePath& filename) const {
  static const std::string empty;
  OriginalDataMap::const_iterator entry = original_data_.find(filename);
  if (entry != original_data_.end()) {
    return entry->second;
  } else {
    return empty;
  }
}

// Import the given COLLADA file or ZIP file under the given parent node.
// Parameters:
//   filename:        The COLLADA or ZIPped COLLADA file to import.
//   parent:          The parent node under which the imported nodes will be
//                    placed. If NULL, nodes will be placed under the
//                    client's root.
//   animation_input: The float parameter used as the input to the animation
//                    (if present). This is usually time. This can be null
//                    if there is no animation.
// Returns true on success.
bool Collada::ImportFile(const FilePath& filename, Transform* parent,
                         ParamFloat* animation_input) {
  // Each time we start a new import, we need to clear out data from
  // the last import (if any).
  ClearData();

  // Convert the base_path given in the options to an absolute path.
  base_path_ = options_.base_path;
  file_util::AbsolutePath(&base_path_);

  bool status = false;
  if (ZipArchive::IsZipFile(FilePathToUTF8(filename))) {
    status = ImportZIP(filename, parent, animation_input);
  } else {
    status = ImportDAE(filename, parent, animation_input);
  }

  if (!status) {
    // TODO(o3d): this could probably be the original URI instead of some
    //     filename in the temp folder.
    O3D_ERROR(service_locator_) << "Unable to import: "
                                << FilePathToUTF8(filename).c_str();
  }

  return status;
}

// Imports the given ZIP file into the given client.
// Parameters:  see Import() above.
// Returns true on success.
bool Collada::ImportZIP(const FilePath& filename, Transform* parent,
                        ParamFloat* animation_input) {
  // This uses minizip, which avoids decompressing the zip archive to a
  // temp directory...
  //
  bool status = false;
  int result = 0;

  String filename_str = FilePathToUTF8(filename);
  collada_zip_archive_ = new ColladaZipArchive(filename_str, &result);

  if (result == UNZ_OK) {
    FCollada::Initialize();
    FCDocument* doc = FCollada::NewTopDocument();

    if (doc) {
      std::string model_path = collada_zip_archive_->GetColladaPath().c_str();

      size_t doc_buffer_size = 0;
      char *doc_buffer = collada_zip_archive_->GetFileData(model_path,
                                                           &doc_buffer_size);

      if (doc_buffer && doc_buffer_size > 0) {
        DLOG(INFO) << "Loading Collada model \""
                   << model_path << "\" from zip file \""
                   << filename_str << "\"";

        std::wstring model_path_w = UTF8ToWide(model_path);

        bool fc_status = FCollada::LoadDocumentFromMemory(model_path_w.c_str(),
                                                          doc,
                                                          doc_buffer,
                                                          doc_buffer_size);


        if (fc_status) {
          if (options_.condition_document) {
            ColladaConditioner conditioner(service_locator_);
            if (conditioner.ConditionDocument(doc, collada_zip_archive_)) {
              status = ImportDAEDocument(doc,
                                         fc_status,
                                         parent,
                                         animation_input);
            }
          } else {
            status = ImportDAEDocument(doc, fc_status, parent, animation_input);
          }
        }
      }
      doc->Release();
    }
    FCollada::Release();
  }

  if (!status) {
    delete collada_zip_archive_;
    collada_zip_archive_ = NULL;
  }

  return status;
}

// Imports the given COLLADA file (.DAE) into the given pack.
// Parameters:
//   see Import() above.
// Returns:
//  true on success.
bool Collada::ImportDAE(const FilePath& filename,
                        Transform* parent,
                        ParamFloat* animation_input) {
  if (!parent) {
    return false;
  }
  bool status = false;
  FCollada::Initialize();
  FCDocument* doc = FCollada::NewTopDocument();
  if (doc) {
    std::wstring filename_w = FilePathToWide(filename);
    bool fc_status = FCollada::LoadDocumentFromFile(doc, filename_w.c_str());
    if (options_.condition_document) {
      ColladaConditioner conditioner(service_locator_);
      if (conditioner.ConditionDocument(doc, NULL)) {
        status = ImportDAEDocument(doc, fc_status, parent, animation_input);
      }
    } else {
      status = ImportDAEDocument(doc, fc_status, parent, animation_input);
    }
    doc->Release();
  }
  FCollada::Release();
  return status;
}

// Imports the given FCDocument document (already loaded) into the given pack.
// Returns:
//  true on success.
bool Collada::ImportDAEDocument(FCDocument* doc,
                                bool fc_status,
                                Transform* parent,
                                ParamFloat* animation_input) {
  if (!parent) {
    return false;
  }
  bool status = false;
  if (doc) {
    if (fc_status) {
      Vector3 up_axis = options_.up_axis;
      FMVector3 up(up_axis.getX(), up_axis.getY(), up_axis.getZ());
      // Transform the document to the given up vector

      FCDocumentTools::StandardizeUpAxisAndLength(doc, up);

      // Import all the textures in the file. Even if they are not used by
      // materials or models the user put them in the file and might need them
      // at runtime.
      //
      // TODO(o3d): Add option to skip this step if user just wants what's
      // actually used by models. The rest of the code already deals with this.
      FCDImageLibrary* image_library = doc->GetImageLibrary();
      for (uint32 i = 0; i < image_library->GetEntityCount(); i++) {
        FCDEntity* entity = image_library->GetEntity(i);
        LOG_ASSERT(entity);
        LOG_ASSERT(entity->GetType() == FCDEntity::IMAGE);
        FCDImage* image = down_cast<FCDImage*>(entity);
        BuildTextureFromImage(image);
      }

      // Import all the materials in the file. Even if they are not used by
      // models the user put them in the file and might need them at runtime.
      //
      // TODO(o3d): Add option to skip this step if user just wants what's
      // actually used by models. The rest of the code already deals with this.
      FCDMaterialLibrary* material_library = doc->GetMaterialLibrary();
      for (uint32 i = 0; i < material_library->GetEntityCount(); i++) {
        FCDEntity* entity = material_library->GetEntity(i);
        LOG_ASSERT(entity);
        LOG_ASSERT(entity->GetType() == FCDEntity::MATERIAL);
        FCDMaterial* collada_material = down_cast<FCDMaterial*>(entity);
        BuildMaterial(doc, collada_material);
      }

      // Import the scene objects, starting at the root.
      FCDSceneNode* scene = doc->GetVisualSceneInstance();
      if (scene) {
        instance_root_ = CreateInstanceTree(scene);
        ImportTree(instance_root_, parent, animation_input);
        ImportTreeInstances(doc, instance_root_);
        delete instance_root_;
        instance_root_ = NULL;
        status = true;
      }
    }
  }
  return status;
}

namespace {

Curve::Infinity ConvertInfinity(FUDaeInfinity::Infinity infinity) {
  switch (infinity) {
    case FUDaeInfinity::LINEAR :
      return Curve::LINEAR;
    case FUDaeInfinity::CYCLE :
      return Curve::CYCLE;
    case FUDaeInfinity::CYCLE_RELATIVE :
      return Curve::CYCLE_RELATIVE;
    case FUDaeInfinity::OSCILLATE :
      return Curve::OSCILLATE;
    default:
      return Curve::CONSTANT;
  }
}

StepCurveKey* BuildStepKey(Curve* curve, FCDAnimationKey* fcd_key,
                           float output_scale) {
  StepCurveKey* key = curve->Create<StepCurveKey>();
  key->SetInput(fcd_key->input);
  key->SetOutput(fcd_key->output * output_scale);
  return key;
}

LinearCurveKey* BuildLinearKey(Curve* curve, FCDAnimationKey* fcd_key,
                               float output_scale) {
  LinearCurveKey* key = curve->Create<LinearCurveKey>();
  key->SetInput(fcd_key->input);
  key->SetOutput(fcd_key->output * output_scale);
  return key;
}

BezierCurveKey* BuildBezierKey(Curve* curve, FCDAnimationKeyBezier* fcd_key,
                               float output_scale) {
  BezierCurveKey* key = curve->Create<BezierCurveKey>();
  key->SetInput(fcd_key->input);
  key->SetOutput(fcd_key->output * output_scale);
  Float2 in_tangent = FMVector2ToFloat2(fcd_key->inTangent);
  in_tangent[1] *= output_scale;
  key->SetInTangent(in_tangent);
  Float2 out_tangent = FMVector2ToFloat2(fcd_key->outTangent);
  out_tangent[1] *= output_scale;
  key->SetOutTangent(out_tangent);
  return key;
}

void BindParams(ParamObject* input_object, const char* input_param_name,
                ParamObject* output_object, const char* output_param_name) {
  Param* input_param = input_object->GetUntypedParam(input_param_name);
  DCHECK(input_param);

  Param* output_param = output_object->GetUntypedParam(output_param_name);
  DCHECK(output_param);

  bool ok = input_param->Bind(output_param);
  DCHECK_EQ(ok, true);
}

void BindParams(ParamObject* input_object, const char* input_param_name,
                Param* output_param) {
  Param* input_param = input_object->GetUntypedParam(input_param_name);
  DCHECK(input_param);

  bool ok = input_param->Bind(output_param);
  DCHECK_EQ(ok, true);
}

void BindParams(Param* input_param,
                ParamObject* output_object, const char* output_param_name) {
  Param* output_param = output_object->GetUntypedParam(output_param_name);
  DCHECK(output_param);

  bool ok = input_param->Bind(output_param);
  DCHECK_EQ(ok, true);
}
}  // namespace anonymous

bool Collada::BuildFloatAnimation(ParamFloat* result,
                                  FCDAnimated* animated,
                                  const char* qualifier,
                                  ParamFloat* animation_input,
                                  float output_scale,
                                  float default_value) {
  if (animated != NULL) {
    FCDAnimationCurve* fcd_curve = animated->FindCurve(qualifier);
    if (fcd_curve != NULL) {
      FunctionEval* function_eval = pack_->Create<FunctionEval>();
      BindParams(function_eval, FunctionEval::kInputParamName, animation_input);

      Curve* curve = pack_->Create<Curve>();
      function_eval->set_function_object(curve);

      curve->set_pre_infinity(ConvertInfinity(fcd_curve->GetPreInfinity()));
      curve->set_post_infinity(ConvertInfinity(fcd_curve->GetPostInfinity()));

      for (int i = 0; i != fcd_curve->GetKeyCount(); ++i) {
        FCDAnimationKey* fcd_key = fcd_curve->GetKey(i);
        switch (fcd_key->interpolation) {
          case FUDaeInterpolation::STEP:
            BuildStepKey(curve, fcd_key, output_scale);
            break;
          case FUDaeInterpolation::BEZIER:
            BuildBezierKey(curve, static_cast<FCDAnimationKeyBezier*>(fcd_key),
                           output_scale);
            break;
          default:
            BuildLinearKey(curve, fcd_key, output_scale);
            break;
        }
      }

      BindParams(result, function_eval, FunctionEval::kOutputParamName);
      return true;
    }
  }

  result->set_value(default_value * output_scale);
  return false;
}

bool Collada::BuildFloat3Animation(ParamFloat3* result, FCDAnimated* animated,
                                   ParamFloat* animation_input,
                                   const Float3& default_value) {
  ParamOp3FloatsToFloat3* to_float3 =
      pack_->Create<ParamOp3FloatsToFloat3>();

  static const char* const kQualifiers[3] = { ".X", ".Y", ".Z" };
  static const char* const kInputs[3] = {
    ParamOp3FloatsToFloat3::kInput0ParamName,
    ParamOp3FloatsToFloat3::kInput1ParamName,
    ParamOp3FloatsToFloat3::kInput2ParamName,
  };
  bool any_animated = false;
  for (int i = 0; i != 3; ++i) {
    ParamFloat* to_float3_input = to_float3->GetParam<ParamFloat>(kInputs[i]);
    any_animated |= BuildFloatAnimation(to_float3_input,
                                        animated, kQualifiers[i],
                                        animation_input, 1.0f,
                                        default_value[i]);
  }

  if (any_animated) {
    BindParams(result, to_float3, ParamOp3FloatsToFloat3::kOutputParamName);
    return true;
  } else {
    pack_->RemoveObject(to_float3);
    result->set_value(default_value);
    return false;
  }
}

ParamMatrix4* Collada::BuildComposition(FCDTMatrix* transform,
                                        ParamMatrix4* input_matrix,
                                        ParamFloat* animation_input) {
  Matrix4Composition* composition = pack_->Create<Matrix4Composition>();
  Matrix4 matrix = FMMatrix44ToMatrix4(transform->ToMatrix());

  ParamOp16FloatsToMatrix4* to_matrix4 =
      pack_->Create<ParamOp16FloatsToMatrix4>();
  bool any_animated = false;
  for (int i = 0; i != 16; ++i) {
    int row = i / 4;
    int column = i % 4;

    std::stringstream input_name;
    input_name << "input" << i;
    ParamFloat* to_matrix4_input = to_matrix4->GetParam<ParamFloat>(
        input_name.str().c_str());

    std::stringstream qualifier;
    qualifier <<  "(" << row << ")" << "(" << column << ")";
    any_animated |= BuildFloatAnimation(to_matrix4_input,
                                        transform->GetAnimated(),
                                        qualifier.str().c_str(),
                                        animation_input, 1.0f,
                                        matrix[row][column]);
  }

  if (any_animated) {
    BindParams(composition, Matrix4Composition::kLocalMatrixParamName,
               to_matrix4, ParamOp16FloatsToMatrix4::kOutputParamName);
  } else {
    pack_->RemoveObject(to_matrix4);
    composition->set_local_matrix(matrix);
  }

  BindParams(composition, Matrix4Composition::kInputMatrixParamName,
             input_matrix);
  return composition->GetParam<ParamMatrix4>(
      Matrix4Composition::kOutputMatrixParamName);
}

ParamMatrix4* Collada::BuildComposition(const Matrix4& matrix,
                                        ParamMatrix4* input_matrix) {
  Matrix4Composition* composition = pack_->Create<Matrix4Composition>();
  composition->set_local_matrix(matrix);
  BindParams(composition, Matrix4Composition::kInputMatrixParamName,
             input_matrix);
  return composition->GetParam<ParamMatrix4>(
      Matrix4Composition::kOutputMatrixParamName);
}

ParamMatrix4* Collada::BuildTranslation(FCDTTranslation* transform,
                                        ParamMatrix4* input_matrix,
                                        ParamFloat* animation_input) {
  Matrix4Translation* translation = pack_->Create<Matrix4Translation>();

  ParamFloat3* animated_param = translation->GetParam<ParamFloat3>(
      Matrix4Translation::kTranslationParamName);
  Vector3 default_value = FMVector3ToVector3(transform->GetTranslation());
  BuildFloat3Animation(animated_param, transform->GetAnimated(),
                       animation_input, Float3(default_value));

  BindParams(translation, Matrix4Composition::kInputMatrixParamName,
             input_matrix);
  return translation->GetParam<ParamMatrix4>(
      Matrix4Composition::kOutputMatrixParamName);
}

ParamMatrix4* Collada::BuildRotation(FCDTRotation* transform,
                                     ParamMatrix4* input_matrix,
                                     ParamFloat* animation_input) {
  Matrix4AxisRotation* rotation = pack_->Create<Matrix4AxisRotation>();

  ParamFloat3* animated_axis = rotation->GetParam<ParamFloat3>(
      Matrix4AxisRotation::kAxisParamName);
  Vector3 default_axis = FMVector3ToVector3(transform->GetAxis());
  BuildFloat3Animation(animated_axis, transform->GetAnimated(),
                       animation_input, Float3(default_axis));

  ParamFloat* animated_angle = rotation->GetParam<ParamFloat>(
      Matrix4AxisRotation::kAngleParamName);
  float default_angle = transform->GetAngle();
  BuildFloatAnimation(animated_angle,
                      transform->GetAnimated(),
                      ".ANGLE",
                      animation_input,
                      o3d::kPi / 180.0f,
                      default_angle);

  BindParams(rotation, Matrix4Composition::kInputMatrixParamName, input_matrix);
  return rotation->GetParam<ParamMatrix4>(
      Matrix4Composition::kOutputMatrixParamName);
}

ParamMatrix4* Collada::BuildScaling(FCDTScale* transform,
                                    ParamMatrix4* input_matrix,
                                    ParamFloat* animation_input) {
  Matrix4Scale* scaling = pack_->Create<Matrix4Scale>();

  ParamFloat3* animated_param = scaling->GetParam<ParamFloat3>(
      Matrix4Scale::kScaleParamName);
  Vector3 default_value = FMVector3ToVector3(transform->GetScale());
  BuildFloat3Animation(animated_param, transform->GetAnimated(),
                       animation_input, Float3(default_value));

  BindParams(scaling, Matrix4Composition::kInputMatrixParamName, input_matrix);
  return scaling->GetParam<ParamMatrix4>(
      Matrix4Composition::kOutputMatrixParamName);
}

// Builds a Transform node corresponding to the transform elements of
// a given node.  All transformations (rotation, translation, scale,
// etc) are collapsed into a single Transform.
// Parameters:
//   node:      The FCollada node whose transforms are replicated.
// Return value:
//   The new Transform.
Transform* Collada::BuildTransform(FCDSceneNode* node,
                                   Transform* parent_transform,
                                   ParamFloat* animation_input) {
  const wchar_t* name = node->GetName().c_str();

  String name_utf8 =  WideToUTF8(name);

  Transform* transform = pack_->Create<Transform>();
  transform->set_name(name_utf8);
  transform->SetParent(parent_transform);

  bool any_animated = false;
  for (size_t i = 0; i < node->GetTransformCount(); i++) {
    FCDTransform* fcd_transform = node->GetTransform(i);
    any_animated |= fcd_transform->IsAnimated();
  }

  if (any_animated) {
    // At least one of the collada transforms is animated so construct the
    // transform hierarchy and connect its output to the local_matrix for
    // this node.
    ParamMatrix4* input_matrix = NULL;
    for (size_t i = 0; i < node->GetTransformCount(); i++) {
      FCDTransform* fcd_transform = node->GetTransform(i);
      switch (fcd_transform->GetType()) {
        case FCDTransform::MATRIX :
          input_matrix = BuildComposition(
              static_cast<FCDTMatrix*>(fcd_transform),
              input_matrix, animation_input);
          break;
        case FCDTransform::TRANSLATION:
          input_matrix = BuildTranslation(
              static_cast<FCDTTranslation*>(fcd_transform),
              input_matrix, animation_input);
          break;
        case FCDTransform::ROTATION:
          input_matrix = BuildRotation(
              static_cast<FCDTRotation*>(fcd_transform),
              input_matrix, animation_input);
          break;
        case FCDTransform::SCALE:
          input_matrix = BuildScaling(
              static_cast<FCDTScale*>(fcd_transform),
              input_matrix, animation_input);
          break;
        default:
          input_matrix = BuildComposition(
              FMMatrix44ToMatrix4(fcd_transform->ToMatrix()),
              input_matrix);
          break;
      }
    }

    ParamMatrix4* local_matrix_param = transform->GetParam<ParamMatrix4>(
        Transform::kLocalMatrixParamName);
    local_matrix_param->Bind(input_matrix);
  } else {
    // None of collada transforms are animated so just compute the
    // overall transform and set it as the value of the local_matrix
    // for this node. This saves memory and improves performance but
    // more importantly it will allow JavaScript code to set the value of
    // the local_matrix directly without first having to unbind it.
    Matrix4 local_matrix(Matrix4::identity());
    for (size_t i = 0; i < node->GetTransformCount(); i++) {
      FCDTransform* fcd_transform = node->GetTransform(i);
      local_matrix *= FMMatrix44ToMatrix4(fcd_transform->ToMatrix());
    }
    transform->set_local_matrix(local_matrix);
  }

  return transform;
}

NodeInstance *Collada::CreateInstanceTree(FCDSceneNode *node) {
  NodeInstance *instance = new NodeInstance(node);
  NodeInstance::NodeInstanceList &children = instance->children();
  for (size_t i = 0; i < node->GetChildrenCount(); ++i) {
    FCDSceneNode *child_node = node->GetChild(i);
    NodeInstance *child_instance = CreateInstanceTree(child_node);
    children.push_back(child_instance);
  }
  return instance;
}

NodeInstance *NodeInstance::FindNodeInTree(FCDSceneNode *node) {
  if (node == node_)
    return this;
  for (unsigned int i = 0; i < children_.size(); ++i) {
    NodeInstance *instance = children_[i]->FindNodeInTree(node);
    if (instance)
      return instance;
  }
  return NULL;
}

NodeInstance *Collada::FindNodeInstance(FCDSceneNode *node) {
  // First try the fast path, in the case where the node is not instanced more
  // than once.
  NodeInstance *instance = FindNodeInstanceFast(node);
  if (!instance) {
    // If it fails, look in the whole instance tree.
    instance = instance_root_->FindNodeInTree(node);
  }
  return instance;
}

NodeInstance *Collada::FindNodeInstanceFast(FCDSceneNode *node) {
  if (node == instance_root_->node())
    return instance_root_;
  // If the node is instanced more than once, fail.
  if (node->GetParentCount() != 1)
    return NULL;
  NodeInstance *parent_instance = FindNodeInstanceFast(node->GetParent(0));
  // Look for self in parent's childrens.
  return parent_instance->FindNodeShallow(node);
}

// Recursively imports a tree of nodes from FCollada, rooted at the
// given node, into the O3D scene.
// Parameters:
//   instance:         The root NodeInstance from which to import.
//   parent_transform: The parent Transform under which the tree will be placed.
//   animation_input:  The float parameter used as the input to the animation
//                     (if present). This is usually time. This can be null
//                     if there is no animation.
void Collada::ImportTree(NodeInstance *instance,
                         Transform* parent_transform,
                         ParamFloat* animation_input) {
  FCDSceneNode *node = instance->node();
  Transform* transform = BuildTransform(node, parent_transform,
                                        animation_input);
  instance->set_transform(transform);

  // recursively import the rest of the nodes in the tree
  const NodeInstance::NodeInstanceList &children = instance->children();
  for (size_t i = 0; i < children.size(); ++i) {
    ImportTree(children[i], transform, animation_input);
  }
}

void Collada::ImportTreeInstances(FCDocument* doc,
                                  NodeInstance *node_instance) {
  FCDSceneNode *node = node_instance->node();
  // recursively import the rest of the nodes in the tree
  const NodeInstance::NodeInstanceList &children = node_instance->children();
  for (size_t i = 0; i < children.size(); ++i) {
    ImportTreeInstances(doc, children[i]);
  }

  Transform* transform = node_instance->transform();
  for (size_t i = 0; i < node->GetInstanceCount(); ++i) {
    FCDEntityInstance* instance = node->GetInstance(i);
    FCDCamera* camera(NULL);
    FCDGeometryInstance* geom_instance(NULL);

    LOG_ASSERT(instance != 0);
    // Import each node based on what kind of entity it is
    // TODO(o3d): add more entity types as they are supported
    switch (instance->GetEntityType()) {
      case FCDEntity::CAMERA:
        // camera entity
        camera = down_cast<FCDCamera*>(instance->GetEntity());
        BuildCamera(doc, camera, transform, node);
        break;
      case FCDEntity::GEOMETRY: {
        // geometry entity
        geom_instance = static_cast<FCDGeometryInstance*>(instance);
        Shape* shape = GetShape(doc, geom_instance);
        if (shape) {
          transform->AddShape(shape);
        }
        break;
      }
      case FCDEntity::CONTROLLER: {
        FCDControllerInstance* controller_instance =
            static_cast<FCDControllerInstance*> (instance);
        Shape* shape = GetSkinnedShape(doc, controller_instance, node_instance);
        if (shape) {
          transform->AddShape(shape);
        }
        break;
      }
      default:
        // do nothing
        break;
    }
  }
}

// Converts an FCollada vertex attribute semantic into an O3D
// vertex attribute semantic.  Returns the O3D semantic, or
// UNKNOWN_SEMANTIC on failure.
static Stream::Semantic C2G3DSemantic(FUDaeGeometryInput::Semantic semantic) {
  switch (semantic) {
    case FUDaeGeometryInput::POSITION: return Stream::POSITION;
    case FUDaeGeometryInput::VERTEX:  return Stream::POSITION;
    case FUDaeGeometryInput::NORMAL:  return Stream::NORMAL;
    case FUDaeGeometryInput::TEXTANGENT:  return Stream::TANGENT;
    case FUDaeGeometryInput::TEXBINORMAL:  return Stream::BINORMAL;
    case FUDaeGeometryInput::TEXCOORD:  return Stream::TEXCOORD;
    case FUDaeGeometryInput::COLOR:  return Stream::COLOR;
    default:  return Stream::UNKNOWN_SEMANTIC;
  }
}

// Imports information from a Collada camera node and places it in Params of
// the transform corresponding to the node it's parented under.
// Parameters:
//   doc:                The FCollada document from which to import nodes.
//   camera:             The FCDCamera node in question
//   transform:          The O3D Transform on which the camera parameters
//                       will be created.  It cannot be NULL.
//   parent_node:        The FCDSceneNode that the parent transform was created
//                       from.  This node is used to extract possible LookAt
//                       elements.  It cannot be NULL.
void Collada::BuildCamera(FCDocument* doc,
                          FCDCamera* camera,
                          Transform* transform,
                          FCDSceneNode* parent_node) {
  LOG_ASSERT(doc && camera && transform && parent_node);

  // Create a transform node for the camera
  String camera_name = WideToUTF8(camera->GetName().c_str());

  // Tag this node as a camera
  ParamString* param_tag = transform->CreateParam<ParamString>(
      COLLADA_STRING_CONSTANT("tags"));

  // Add more tags when required
  param_tag->set_value("camera");

  // Create the rest of the params

  // Projection type: either 'orthographic' or 'perspective'
  ParamString* param_proj_type =
      transform->CreateParam<ParamString>(
          COLLADA_STRING_CONSTANT("projectionType"));

  // Aspect ratio
  float camera_aspect_ratio;
  ParamFloat* param_proj_aspect_ratio =
      transform->CreateParam<ParamFloat>(
          COLLADA_STRING_CONSTANT("projectionAspectRatio"));

  // Near/far z-planes
  float camera_near_z, camera_far_z;
  ParamFloat* param_proj_nearz =
      transform->CreateParam<ParamFloat>(
          COLLADA_STRING_CONSTANT("projectionNearZ"));
  ParamFloat* param_proj_farz =
      transform->CreateParam<ParamFloat>(
          COLLADA_STRING_CONSTANT("projectionFarZ"));

  // Calculate shared params
  camera_near_z = camera->GetNearZ();
  camera_far_z = camera->GetFarZ();
  param_proj_nearz->set_value(camera_near_z);
  param_proj_farz->set_value(camera_far_z);

  if (camera->GetProjectionType() == FCDCamera::ORTHOGRAPHIC) {
    // Orthographic projection
    param_proj_type->set_value("orthographic");

    // Additional params
    // Horizontal and vertical magnifications
    ParamFloat* param_proj_mag_x =
        transform->CreateParam<ParamFloat>(
            COLLADA_STRING_CONSTANT("projectionMagX"));
    ParamFloat* param_proj_mag_y =
        transform->CreateParam<ParamFloat>(
            COLLADA_STRING_CONSTANT("projectionMagY"));

    // Find aspect ratio and magnifications
    float camera_mag_x, camera_mag_y;
    if (camera->HasHorizontalMag() && camera->HasVerticalMag())
      camera_aspect_ratio = camera->GetMagY() / camera->GetMagX();
    else
      camera_aspect_ratio = camera->GetAspectRatio();

    if (camera->HasHorizontalMag())
      camera_mag_x = camera->GetMagX();
    else
      camera_mag_x = camera->GetMagY() * camera_aspect_ratio;

    if (camera->HasVerticalMag())
      camera_mag_y = camera->GetMagY();
    else
      camera_mag_y = camera->GetMagX() / camera_aspect_ratio;

    // Set the values of the additional params
    param_proj_mag_x->set_value(camera_mag_x);
    param_proj_mag_y->set_value(camera_mag_y);
    param_proj_aspect_ratio->set_value(camera_aspect_ratio);
  } else if (camera->GetProjectionType() == FCDCamera::PERSPECTIVE) {
    // Perspective projection
    param_proj_type->set_value("perspective");

    // Additional params
    // Vertical field of view
    ParamFloat* param_proj_fov_y =
        transform->CreateParam<ParamFloat>(
            COLLADA_STRING_CONSTANT("perspectiveFovY"));

    // Find aspect ratio and vertical FOV
    float camera_fov_y;
    if (camera->HasHorizontalFov() && camera->HasVerticalFov())
      camera_aspect_ratio = camera->GetFovY() / camera->GetFovX();
    else
      camera_aspect_ratio = camera->GetAspectRatio();

    if (camera->HasVerticalFov())
      camera_fov_y = camera->GetFovY();
    else
      camera_fov_y = camera->GetFovX() / camera_aspect_ratio;

    // Set the values of the additional params
    param_proj_fov_y->set_value(camera_fov_y);
    param_proj_aspect_ratio->set_value(camera_aspect_ratio);
  }

  // Search the FCDSceneNode for a LookAt element, extract the eye, target,
  // and up values and store them as Params on the Transform.  If multiple
  // LookAt elements are defined under the parent node, we only pick the first
  // one.
  for (size_t i = 0; i < parent_node->GetTransformCount(); i++) {
    FCDTransform* transform_object = parent_node->GetTransform(i);
    if (transform_object->GetType() == FCDTransform::LOOKAT) {
      FCDTLookAt* look_at = static_cast<FCDTLookAt*>(transform_object);
      FMVector3 position = look_at->GetPosition();
      FMVector3 target = look_at->GetTarget();
      FMVector3 up = look_at->GetUp();

      // Get the world matrix of the transform above the camera transform.
      // We use this value to transform the eye, target and up to the world
      // coordinate system so that they can be used directly to make a camera
      // view matrix.
      Matrix4 parent_world = Matrix4::identity();
      if (transform->parent())
        parent_world = transform->parent()->GetUpdatedWorldMatrix();

      ParamFloat3* param_eye_position =
          transform->CreateParam<ParamFloat3>(
              COLLADA_STRING_CONSTANT("eyePosition"));
      Vector4 world_eye = parent_world * Point3(position.x,
                                                position.y,
                                                position.z);
      param_eye_position->set_value(Float3(world_eye.getX(),
                                           world_eye.getY(),
                                           world_eye.getZ()));

      ParamFloat3* param_target_position =
          transform->CreateParam<ParamFloat3>(
              COLLADA_STRING_CONSTANT("targetPosition"));
      Vector4 world_target = parent_world * Point3(target.x,
                                                   target.y,
                                                   target.z);
      param_target_position->set_value(Float3(world_target.getX(),
                                              world_target.getY(),
                                              world_target.getZ()));

      ParamFloat3* param_up_vector =
          transform->CreateParam<ParamFloat3>(
              COLLADA_STRING_CONSTANT("upVector"));
      Vector4 world_up = parent_world * Vector4(up.x, up.y, up.z, 0.0f);
      param_up_vector->set_value(Float3(world_up.getX(),
                                        world_up.getY(),
                                        world_up.getZ()));

      break;
    }
  }
}

Shape* Collada::GetShape(FCDocument* doc,
                         FCDGeometryInstance* geom_instance) {
  Shape* shape = NULL;
  FCDGeometry* geom = static_cast<FCDGeometry*>(geom_instance->GetEntity());
  if (geom) {
    fm::string geom_id = geom->GetDaeId();
    shape = shapes_[geom_id.c_str()];
    if (!shape) {
      shape = BuildShape(doc, geom_instance, geom, NULL);
      if (!shape)
        return NULL;
      shapes_[geom_id.c_str()] = shape;
    }
  }
  return shape;
}

Shape* Collada::GetSkinnedShape(FCDocument* doc,
                                FCDControllerInstance* instance,
                                NodeInstance *parent_node_instance) {
  Shape* shape = NULL;
  FCDController* controller =
      static_cast<FCDController*>(instance->GetEntity());
  if (controller && controller->IsSkin()) {
    fm::string id = controller->GetDaeId();
    shape = skinned_shapes_[id.c_str()];
    if (!shape) {
      shape = BuildSkinnedShape(doc, instance, parent_node_instance);
      if (!shape)
        return NULL;
      skinned_shapes_[id.c_str()] = shape;
    }
  }
  return shape;
}

// Builds an O3D shape node corresponding to a given FCollada geometry
// instance.
// Parameters:
//   doc:                The FCollada document from which to import nodes.
//   geom_instance:      The FCollada geometry node to be converted.
Shape* Collada::BuildShape(FCDocument* doc,
                           FCDGeometryInstance* geom_instance,
                           FCDGeometry* geom,
                           TranslationMap* translationMap) {
  Shape* shape = NULL;
  LOG_ASSERT(doc && geom_instance && geom);
  if (geom && geom->IsMesh()) {
    String geom_name = WideToUTF8(geom->GetName().c_str());
    shape = pack_->Create<Shape>();
    shape->set_name(geom_name);
    FCDGeometryMesh* mesh = geom->GetMesh();
    LOG_ASSERT(mesh);
    FCDGeometryPolygonsTools::Triangulate(mesh);
    FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh, NULL, translationMap);
    size_t num_polygons = mesh->GetPolygonsCount();
    size_t num_indices = mesh->GetFaceVertexCount();
    if (num_polygons <= 0 || num_indices <= 0) return NULL;
    FCDGeometrySource* pos_source =
        mesh->FindSourceByType(FUDaeGeometryInput::POSITION);
    if (pos_source == NULL) return NULL;
    size_t num_vertices = pos_source->GetValueCount();
    size_t num_sources = mesh->GetSourceCount();

    // Create vertex streams corresponding to the COLLADA sources.
    // These streams are common to all polygon sets in this mesh.
    // The BuildSkinnedShape code assumes this so if you change it you'll need
    // to fix BuildSkinnedShape.
    StreamBank* stream_bank = pack_->Create<StreamBank>();
    stream_bank->set_name(geom_name);

    int semantic_counts[Stream::TEXCOORD + 1] = { 0, };

    scoped_array<Field*> fields(new Field*[num_sources]);

    VertexBuffer* vertex_buffer = pack_->Create<VertexBuffer>();
    vertex_buffer->set_name(geom_name);

    // first create all the fields.
    for (size_t s = 0; s < num_sources; ++s) {
      FCDGeometrySource* source = mesh->GetSource(s);
      LOG_ASSERT(source);
      Stream::Semantic semantic = C2G3DSemantic(source->GetType());
      LOG_ASSERT(semantic <= Stream::TEXCOORD);
      if (semantic == Stream::UNKNOWN_SEMANTIC) continue;

      // The call to GenerateUniqueIndices() above should have made
      // all sources the same length.
      LOG_ASSERT(source->GetValueCount() == num_vertices);

      int stride = source->GetStride();
      if (semantic == Stream::COLOR && stride == 4) {
        fields[s] = vertex_buffer->CreateField(UByteNField::GetApparentClass(),
                                               stride);
      } else {
        fields[s] = vertex_buffer->CreateField(FloatField::GetApparentClass(),
                                               stride);
      }
    }

    if (!vertex_buffer->AllocateElements(num_vertices)) {
      O3D_ERROR(service_locator_) << "Failed to allocate vertex buffer";
      return NULL;
    }

    for (size_t s = 0; s < num_sources; ++s) {
      FCDGeometrySource* source = mesh->GetSource(s);
      LOG_ASSERT(source);
      Stream::Semantic semantic = C2G3DSemantic(source->GetType());
      LOG_ASSERT(semantic <= Stream::TEXCOORD);
      if (semantic == Stream::UNKNOWN_SEMANTIC) continue;
      int stride = source->GetStride();

      const float* source_data = source->GetData();
      if (semantic == Stream::TANGENT || semantic == Stream::BINORMAL) {
        // FCollada uses the convention that the tangent points
        // along -u and the binormal along -v in model space.
        // Convert to the more common convention where the tangent
        // points along +u and the binormal along +v. This is what, for
        // example, tools that convert height maps to normal maps tend to
        // assume. It is also what the O3D shaders assume.
        size_t num_values = source->GetDataCount();
        scoped_array<float>values(new float[num_values]);
        for (size_t i = 0; i < num_values ; ++i) {
          values[i] = -source_data[i];
        }
        fields[s]->SetFromFloats(&values[0], stride, 0, num_vertices);
      } else {
        fields[s]->SetFromFloats(source_data, stride, 0, num_vertices);
      }

      stream_bank->SetVertexStream(semantic, semantic_counts[semantic],
                                   fields[s], 0);
      // NOTE: This doesn't really seem like the correct thing to do but I'm not
      // sure we have enough info to do the correct thing. The issue is we
      // need to connect these streams to the shader, the shader needs to know
      // which streams go with which varying parameters but for the standard
      // collada materials I don't think any such information is available.
      ++semantic_counts[semantic];
    }

    for (size_t p = 0; p < num_polygons; ++p) {
      FCDGeometryPolygons* polys = mesh->GetPolygons(p);
      FCDGeometryPolygonsInput* input = polys->GetInput(0);

      size_t size = input->GetIndexCount();
      size_t vertices_per_primitive = 0;
      Primitive::PrimitiveType primitive_type;
      switch (polys->GetPrimitiveType()) {
        case FCDGeometryPolygons::POLYGONS:
          vertices_per_primitive = 3;
          primitive_type = Primitive::TRIANGLELIST;
          break;
        case FCDGeometryPolygons::LINES:
          vertices_per_primitive = 2;
          primitive_type = Primitive::LINELIST;
          break;
        default:
          // unsupported geometry type; skip it
          continue;
      }

      // If there are no vertices, don't make this primitive.
      if (size == 0) continue;

      // If we don't have a multiple of the verts-per-primitive, bail now.
      if (size % vertices_per_primitive != 0) {
        O3D_ERROR(service_locator_) << "Geometry \"" << geom_name
                                    << "\" contains " << size
                                    << " vertices, which is not a multiple of "
                                    << vertices_per_primitive << "; skipped";
        continue;
      }

      // Get the material for this polygon set.
      Material* material = NULL;
      FCDMaterialInstance* mat_instance = geom_instance->FindMaterialInstance(
          polys->GetMaterialSemantic());
      if (mat_instance) {
        FCDMaterial* collada_material = mat_instance->GetMaterial();
        material = BuildMaterial(doc, collada_material);
      }
      if (!material) {
        material = GetDummyMaterial();
      }
      // Create an index buffer for this group of polygons.

      String primitive_name(geom_name + "|" + material->name());

      IndexBuffer* indexBuffer = pack_->Create<IndexBuffer>();
      indexBuffer->set_name(primitive_name);
      if (!indexBuffer->AllocateElements(size)) {
        O3D_ERROR(service_locator_) << "Failed to allocate index buffer.";
        return NULL;
      }
      indexBuffer->index_field()->SetFromUInt32s(input->GetIndices(), 1, 0,
                                                 size);

      // Create a primitive for this group of polygons.
      Primitive* primitive = pack_->Create<Primitive>();
      primitive->set_name(primitive_name);

      primitive->set_material(material);

      primitive->SetOwner(shape);
      primitive->set_primitive_type(primitive_type);
      size_t num_prims = size / vertices_per_primitive;
      primitive->set_number_primitives(static_cast<unsigned int>(num_prims));
      primitive->set_number_vertices(static_cast<unsigned int>(num_vertices));

      // Set the index buffer for this primitive.
      primitive->set_index_buffer(indexBuffer);

      // Set the vertex streams for this primitive to the common set for this
      // mesh.
      primitive->set_stream_bank(stream_bank);
    }
  }
  return shape;
}

Shape* Collada::BuildSkinnedShape(FCDocument* doc,
                                  FCDControllerInstance* instance,
                                  NodeInstance *parent_node_instance) {
  // TODO(o3d): Handle chained controllers. Morph->Skin->...
  Shape* shape = NULL;
  LOG_ASSERT(doc && instance);
  FCDController* controller =
      static_cast<FCDController*>(instance->GetEntity());
  if (controller && controller->IsSkin()) {
    FCDSkinController* skin_controller = controller->GetSkinController();
    TranslationMap translationMap;
    shape = BuildShape(doc,
                       instance,
                       controller->GetBaseGeometry(),
                       &translationMap);
    if (!shape) {
      return NULL;
    }

    // Convert the translation table to new->old map.
    size_t num_vertices = 0;
    std::vector<unsigned> new_to_old_indices;
    {
      // walk the map once to figure out the number of vertices.
      TranslationMap::iterator end = translationMap.end();
      for (TranslationMap::iterator it = translationMap.begin();
           it != end;
           ++it) {
        num_vertices += it->second.size();
      }

      // Init our array to UINT_MAX so we can check for collisions
      new_to_old_indices.resize(num_vertices, UINT_MAX);

      // walk the map again and fill out our remap table.
      for (TranslationMap::iterator it = translationMap.begin();
           it != end;
           ++it) {
        const UInt32List& intlist = it->second;
        for (size_t gg = 0; gg < intlist.size(); ++gg) {
          unsigned new_index = intlist[gg];
          LOG_ASSERT(new_to_old_indices.at(new_index) == UINT_MAX);
          new_to_old_indices[new_index] = it->first;
        }
      }
    }

    // There's a BIG assumption here. We assume the first primitive on the shape
    // has vertex buffers that are shared on all the primitives under this shape
    // such that we only need to copy the first primitive's vertex buffers to
    // skin everything. This is actually what was BuildShape was doing at the
    // time this code was written.
    const ElementRefArray& elements = shape->GetElementRefs();
    if (elements.empty() || !elements[0]->IsA(Primitive::GetApparentClass())) {
      return NULL;
    }
    Primitive* primitive = down_cast<Primitive*>(elements[0].Get());

    String controller_name = WideToUTF8(controller->GetName().c_str());

    ParamArray* matrices = pack_->Create<ParamArray>();
    Skin* skin = pack_->Create<Skin>();
    skin->set_name(controller_name);
    SkinEval* skin_eval = pack_->Create<SkinEval>();
    skin_eval->set_name(controller_name);

    skin_eval->set_skin(skin);
    skin_eval->set_matrices(matrices);

    // Bind bones to matrices
    size_t num_bones = instance->GetJointCount();
    if (num_bones > 0) {
      matrices->CreateParam<ParamMatrix4>(num_bones - 1);
      for (size_t ii = 0; ii < num_bones; ++ii) {
        FCDSceneNode* node = instance->GetJoint(ii);
        LOG_ASSERT(node);
        // Note: in case of instancing, the intended instance is ill-defined,
        // but that is a problem in the Collada document itself. So we'll assume
        // the file is somewhat well defined.
        // First we try the single instance case.
        NodeInstance *node_instance = FindNodeInstanceFast(node);
        if (!node_instance) {
          // Second we try nodes underneath the same parent as the controller
          // instance. Max and Maya seem to do that.
          node_instance = parent_node_instance->FindNodeInTree(node);
        }
        if (!node_instance) {
          // Third we try in the entire tree.
          node_instance = instance_root_->FindNodeInTree(node);
        }
        if (!node_instance) {
          String bone_name = WideToUTF8(node->GetName().c_str());
          O3D_ERROR(service_locator_)
              << "Could not find node instance for bone " << bone_name.c_str();
          continue;
        }
        Transform* bone = node_instance->transform();
        LOG_ASSERT(bone);
        matrices->GetUntypedParam(ii)->Bind(
            bone->GetUntypedParam(Transform::kWorldMatrixParamName));
      }
    }

    Matrix4 bind_shape_matrix(
        FMMatrix44ToMatrix4(skin_controller->GetBindShapeTransform()));
    Matrix4 inverse_bind_shape_matrix(inverse(bind_shape_matrix));

    // Get the bind pose inverse matrices
    LOG_ASSERT(num_bones == skin_controller->GetJointCount());
    for (size_t ii = 0; ii < num_bones; ++ii) {
      FCDSkinControllerJoint* joint = skin_controller->GetJoint(ii);
      skin->SetInverseBindPoseMatrix(
          ii, FMMatrix44ToMatrix4(joint->GetBindPoseInverse()));
    }

    // Get Influences.
    for (size_t ii = 0; ii < num_vertices; ++ii) {
      unsigned old_index = new_to_old_indices[ii];
      FCDSkinControllerVertex* vertex =
          skin_controller->GetVertexInfluence(old_index);
      Skin::Influences influences;
      size_t num_influences = vertex->GetPairCount();
      for (size_t jj = 0; jj < num_influences; ++jj) {
        FCDJointWeightPair* weight_pair = vertex->GetPair(jj);
        influences.push_back(Skin::Influence(weight_pair->jointIndex,
                                             weight_pair->weight));
      }
      skin->SetVertexInfluences(ii, influences);
    }

    Matrix4 matrix(bind_shape_matrix);

    // Copy shape->primitive buffers.
    // Here we need to also split the original vertex buffer. The issue is
    // the original VertexBuffer might contain POSITION, NORMAL, TEXCOORD,
    // COLOR. Of those, only POSITION and NORMAL are copied to the SourceBuffer.
    // The VertexBuffer still contains POSITON, NORMAL, TEXCOORD, and COLOR so
    // two issues come up
    //
    // 1) If we serialize that VertexBuffer, POSITION and NORMAL are stored
    // twice. Once in the SourceBuffer, again in the VertexBuffer. That's a lot
    // of data to download just to throw it away.
    //
    // 2) If we want to instance the skin we'll need to make a new VertexBuffer
    // so we can store the skinned vertices for the second instance. But we'd
    // like to share the COLOR and TEXCOORDS. To do that they need to be in
    // a separate VertexBuffer.
    StreamBank* stream_bank = primitive->stream_bank();
    SourceBuffer* buffer = pack_->Create<SourceBuffer>();
    VertexBuffer* shared_buffer = pack_->Create<VertexBuffer>();
    const StreamParamVector& source_stream_params =
        stream_bank->vertex_stream_params();
    std::vector<Field*> new_fields(source_stream_params.size(), NULL);
    // first make all the fields.
    for (unsigned ii = 0; ii < source_stream_params.size(); ++ii) {
      const Stream& source_stream = source_stream_params[ii]->stream();
      const Field& field = source_stream.field();
      bool copied = false;
      if (field.IsA(FloatField::GetApparentClass()) &&
          (field.num_components() == 3 ||
           field.num_components() == 4)) {
        switch (source_stream.semantic()) {
          case Stream::POSITION:
          case Stream::NORMAL:
          case Stream::BINORMAL:
          case Stream::TANGENT: {
            copied = true;
            unsigned num_source_components = field.num_components();
            unsigned num_source_vertices = source_stream.GetMaxVertices();
            if (num_source_vertices != num_vertices) {
              O3D_ERROR(service_locator_)
                  << "Number of vertices in stream_bank '"
                  << stream_bank->name().c_str()
                  << "' does not equal the number of vertices in the Skin '"
                  << skin->name().c_str() << "'";
              return NULL;
            }
            new_fields[ii] = buffer->CreateField(FloatField::GetApparentClass(),
                                                 num_source_components);
          }
        }
      }
      if (!copied) {
        // It's a shared field, copy it to the shared buffer.
        new_fields[ii] = shared_buffer->CreateField(field.GetClass(),
                                                    field.num_components());
      }
    }

    if (!buffer->AllocateElements(num_vertices) ||
        !shared_buffer->AllocateElements(num_vertices)) {
      O3D_ERROR(service_locator_)
          << "Failed to allocate destination vertex buffer";
      return NULL;
    }

    for (unsigned ii = 0; ii < source_stream_params.size(); ++ii) {
      const Stream& source_stream = source_stream_params[ii]->stream();
      const Field& field = source_stream.field();
      bool copied = false;
      if (field.IsA(FloatField::GetApparentClass()) &&
          (field.num_components() == 3 ||
           field.num_components() == 4)) {
        switch (source_stream.semantic()) {
          case Stream::POSITION:
          case Stream::NORMAL:
          case Stream::BINORMAL:
          case Stream::TANGENT: {
            copied = true;
            unsigned num_source_components = field.num_components();
            Field* new_field = new_fields[ii];

            std::vector<float> data(num_vertices * num_source_components);
            field.GetAsFloats(0, &data[0], num_source_components,
                              num_vertices);
            // TODO(o3d): Remove this matrix multiply. I don't think it is
            //     needed.
            for (unsigned vv = 0; vv < num_vertices; ++vv) {
              float* values = &data[vv * num_source_components];
              switch (field.num_components()) {
                case 3: {
                  if (source_stream.semantic() == Stream::POSITION) {
                    Vector4 result(matrix * Point3(values[0],
                                                   values[1],
                                                   values[2]));
                    values[0] = result.getElem(0);
                    values[1] = result.getElem(1);
                    values[2] = result.getElem(2);
                  } else {
                    Vector4 result(matrix * Vector3(values[0],
                                                    values[1],
                                                    values[2]));
                    values[0] = result.getElem(0);
                    values[1] = result.getElem(1);
                    values[2] = result.getElem(2);
                  }
                  break;
                }
                case 4: {
                  Vector4 result(matrix * Vector4(values[0],
                                                  values[1],
                                                  values[2],
                                                  values[3]));
                  values[0] = result.getElem(0);
                  values[1] = result.getElem(1);
                  values[2] = result.getElem(2);
                  values[3] = result.getElem(3);
                  break;
                }
              }
            }
            new_field->SetFromFloats(&data[0], num_source_components, 0,
                                     num_vertices);
            // Bind streams
            skin_eval->SetVertexStream(source_stream.semantic(),
                                       source_stream.semantic_index(),
                                       new_field,
                                       0);
            stream_bank->BindStream(skin_eval,
                                    source_stream.semantic(),
                                    source_stream.semantic_index());
            break;
          }
        }
      }
      if (!copied) {
        Field* new_field = new_fields[ii];
        new_field->Copy(field);
        field.buffer()->RemoveField(&source_stream.field());
        stream_bank->SetVertexStream(source_stream.semantic(),
                                     source_stream.semantic_index(),
                                     new_field,
                                     0);
      }
    }
  }
  return shape;
}

Texture* Collada::BuildTextureFromImage(FCDImage* image) {
  const fstring filename = image->GetFilename();
  Texture* tex = textures_[filename.c_str()];
  if (!tex) {
    FilePath file_path = WideToFilePath(filename.c_str());
    FilePath uri = file_path;

    std::string tempfile;
    if (collada_zip_archive_) {
      // If we're getting data from a zip archive, then we just strip
      // the "/" from the beginning of the name, since that represents
      // the root of the archive, and we can assume all the paths in
      // the archive are relative to that.
      if (uri.value()[0] == FILE_PATH_LITERAL('/')) {
        uri = FilePath(uri.value().substr(1));
      }
      // NOTE: We have the opportunity to simply extract a memory
      // buffer for the image data here, but currently the image loaders expect
      // to read a file, so we write out a temp file...

      // filename_utf8 points to the name of the file inside the archive
      // (it doesn't actually live on the filesystem so we make a temp file)
      if (collada_zip_archive_->GetTempFileFromFile(FilePathToUTF8(file_path),
                                                    &tempfile)) {
        file_path = UTF8ToFilePath(tempfile);
      }
    } else {
      GetRelativePathIfPossible(base_path_, uri, &uri);
    }

    tex = Texture::Ref(
        pack_->CreateTextureFromFile(FilePathToUTF8(uri),
                                     file_path,
                                     Bitmap::UNKNOWN,
                                     options_.generate_mipmaps));
    if (tex) {
      const fstring name(image->GetName());
      tex->set_name(WideToUTF8(name.c_str()));
    }

    if (options_.keep_original_data) {
      // Cache the original data by URI so we can recover it later.
      std::string contents;
      file_util::ReadFileToString(file_path, &contents);
      original_data_[uri] = contents;
    }

    if (tempfile.size() > 0) ZipArchive::DeleteFile(tempfile);
    textures_[filename.c_str()] = tex;
  }
  return tex;
}

Texture* Collada::BuildTexture(FCDEffectParameterSurface* surface) {
  // Create the texture.
  Texture* tex = NULL;
  if (surface->GetImageCount() > 0) {
    FCDImage* image = surface->GetImage(0);
    tex = BuildTextureFromImage(image);
  }
  return tex;
}

// Sets an O3D parameter value from a given FCollada effect parameter.
// instance.
// Parameters:
//   param_object:       The param_object whose parameter is to be modified.
//   param_name:         The name of the parameter to be set.
//   fc_param:           The FCollada effect parameter whose value is used.
// Returns true on success.
bool Collada::SetParamFromFCEffectParam(ParamObject* param_object,
                                        const String &param_name,
                                        FCDEffectParameter *fc_param) {
  if (!param_object || !fc_param) return false;
  FCDEffectParameter::Type type = fc_param->GetType();
  bool rc = false;
  if (type == FCDEffectParameter::FLOAT) {
    FCDEffectParameterFloat* float_param =
        static_cast<FCDEffectParameterFloat*>(fc_param);
    ParamFloat* param = param_object->CreateParam<ParamFloat>(param_name);
    if (param) {
      param->set_value(float_param->GetValue());
      rc = true;
    }
  } else if (type == FCDEffectParameter::FLOAT2) {
    FCDEffectParameterFloat2* float2_param =
        static_cast<FCDEffectParameterFloat2*>(fc_param);
    FMVector2 v = float2_param->GetValue();
    ParamFloat2* param = param_object->CreateParam<ParamFloat2>(param_name);
    if (param) {
      Float2 f(v.x, v.y);
      param->set_value(f);
      rc = true;
    }
  } else if (type == FCDEffectParameter::FLOAT3) {
    FCDEffectParameterFloat3* float3_param =
        static_cast<FCDEffectParameterFloat3*>(fc_param);
    FMVector3 v = float3_param->GetValue();
    ParamFloat3* param = param_object->CreateParam<ParamFloat3>(param_name);
    if (!param)
      return false;
    Float3 f(v.x, v.y, v.z);
    param->set_value(f);
    rc = true;
  } else if (type == FCDEffectParameter::VECTOR) {
    FCDEffectParameterVector* vector_param =
        static_cast<FCDEffectParameterVector*>(fc_param);
    FMVector4 v = vector_param->GetValue();
    ParamFloat4* param = param_object->CreateParam<ParamFloat4>(param_name);
    if (param) {
      Float4 f(v.x, v.y, v.z, v.w);
      param->set_value(f);
      rc = true;
    }
  } else if (type == FCDEffectParameter::INTEGER) {
    FCDEffectParameterInt* int_param =
        static_cast<FCDEffectParameterInt*>(fc_param);
    ParamInteger* param = param_object->CreateParam<ParamInteger>(param_name);
    if (param) {
      param->set_value(int_param->GetValue());
      rc = true;
    }
  } else if (type == FCDEffectParameter::BOOLEAN) {
    FCDEffectParameterBool* bool_param =
        static_cast<FCDEffectParameterBool*>(fc_param);
    ParamBoolean* param = param_object->CreateParam<ParamBoolean>(param_name);
    if (param) {
      param->set_value(bool_param->GetValue());
      rc = true;
    }
  } else if (type == FCDEffectParameter::MATRIX) {
    FCDEffectParameterMatrix* matrix_param =
        static_cast<FCDEffectParameterMatrix*>(fc_param);
    ParamMatrix4* param = param_object->CreateParam<ParamMatrix4>(param_name);
    if (param) {
      param->set_value(FMMatrix44ToMatrix4(matrix_param->GetValue()));
      rc = true;
    }
  } else if (type == FCDEffectParameter::SAMPLER) {
    FCDEffectParameterSampler* sampler =
        static_cast<FCDEffectParameterSampler*>(fc_param);
    ParamSampler* sampler_param = param_object->CreateParam<ParamSampler>(
        param_name);
    if (sampler_param) {
      Sampler* o3d_sampler = pack_->Create<Sampler>();
      o3d_sampler->set_name(param_name);
      sampler_param->set_value(o3d_sampler);

      FCDEffectParameterSurface* surface = sampler->GetSurface();
      if (surface) {
        Texture* tex = BuildTexture(surface);

        // Set the texture on the sampler.
        if (tex) {
          o3d_sampler->set_texture(tex);
          rc = true;
        }
      }
      SetSamplerStates(sampler, o3d_sampler);
    }
  } else if (type == FCDEffectParameter::SURFACE) {
    // TODO(o3d): This code is here to handle the NV_import profile
    // exported by Max's DirectX Shader materials, which references
    // only references texture params (not samplers).  Once we move
    // completely to using samplers and add sampler blocks to our
    // collada file then we should eliminate this codepath.

    FCDEffectParameterSurface* surface =
        static_cast<FCDEffectParameterSurface*>(fc_param);

    Texture* tex = BuildTexture(surface);
    if (tex) {
      ParamTexture* param = param_object->CreateParam<ParamTexture>(param_name);
      if (param) {
        param->set_value(tex);
        rc = true;
      }
    }
  }
  return rc;
}

Effect* Collada::GetDummyEffect() {
  if (!dummy_effect_) {
    // Create a dummy effect, just so we can see something
    dummy_effect_ = pack_->Create<Effect>();
    dummy_effect_->set_name(
        COLLADA_STRING_CONSTANT("substituteForMissingOrBadEffect"));
    dummy_effect_->LoadFromFXString(
        "float4x4 worldViewProj : WorldViewProjection;"
        "float4 vs(float4 v : POSITION ) : POSITION {"
        "  return mul(v, worldViewProj);"
        "}"
        "float4 ps() : COLOR {"
        "  return float4(1, 0, 1, 1);"
        "}\n"
        "// #o3d VertexShaderEntryPoint vs\n"
        "// #o3d PixelShaderEntryPoint ps\n");
  }
  return dummy_effect_;
}

Material* Collada::GetDummyMaterial() {
  if (!dummy_material_) {
    dummy_material_ = pack_->Create<Material>();
    dummy_material_->set_name(
        COLLADA_STRING_CONSTANT("substituteForMissingOrBadMaterial"));
    dummy_material_->set_effect(GetDummyEffect());
  }
  return dummy_material_;
}

static const char* GetLightingType(FCDEffectStandard* std_profile) {
  FCDEffectStandard::LightingType type = std_profile->GetLightingType();
  switch (type) {
    case FCDEffectStandard::CONSTANT:
      return Collada::kLightingTypeConstant;
    case FCDEffectStandard::PHONG:
      return Collada::kLightingTypePhong;
    case FCDEffectStandard::BLINN:
      return Collada::kLightingTypeBlinn;
    case FCDEffectStandard::LAMBERT:
      return Collada::kLightingTypeLambert;
    default:
      return Collada::kLightingTypeUnknown;
  }
}

static FCDEffectProfileFX* FindProfileFX(FCDEffect* effect) {
  FCDEffectProfile* profile = effect->FindProfile(FUDaeProfileType::HLSL);
  if (!profile) {
    profile = effect->FindProfile(FUDaeProfileType::CG);
    if (!profile) {
      return NULL;
    }
  }
  return static_cast<FCDEffectProfileFX*>(profile);
}

Material* Collada::BuildMaterial(FCDocument* doc,
                                 FCDMaterial* collada_material) {
  if (!doc || !collada_material) {
    return NULL;
  }

  fm::string material_id = collada_material->GetDaeId();
  Material* material = materials_[material_id.c_str()];
  if (!material) {
    Effect* effect = NULL;
    FCDEffect* collada_effect = collada_material->GetEffect();
    if (collada_effect) {
      effect = GetEffect(doc, collada_effect);
    }

    String collada_material_name = WideToUTF8(
        collada_material->GetName().c_str());
    Material* material = pack_->Create<Material>();
    material->set_name(collada_material_name);
    material->set_effect(effect);
    SetParamsFromMaterial(collada_material, material);

    // If this is a COLLADA-FX profile, add the render states from the
    // COLLADA-FX sections.
    FCDEffectProfileFX* profile_fx = FindProfileFX(collada_effect);
    if (profile_fx) {
      if (profile_fx->GetTechniqueCount() > 0) {
        FCDEffectTechnique* technique = profile_fx->GetTechnique(0);
        if (technique->GetPassCount() > 0) {
          FCDEffectPass* pass = technique->GetPass(0);
          State* state = pack_->Create<State>();
          state->set_name("pass_state");
          cull_enabled_ = cull_front_ = front_cw_ = false;
          for (size_t i = 0; i < pass->GetRenderStateCount(); ++i) {
            AddRenderState(pass->GetRenderState(i), state);
          }
          material->set_state(state);
        }
      }
    } else {
      FCDEffectStandard* std_profile = static_cast<FCDEffectStandard *>(
          collada_effect->FindProfile(FUDaeProfileType::COMMON));
      if (std_profile) {
        ParamString* type_tag = material->CreateParam<ParamString>(
            kLightingTypeParamName);
        type_tag->set_value(GetLightingType(std_profile));
      }
    }
    materials_[material_id.c_str()] = material;
  }
  return material;
}

Effect* Collada::GetEffect(FCDocument* doc, FCDEffect* collada_effect) {
  Effect* effect = NULL;
  fm::string effect_id = collada_effect->GetDaeId();
  effect = effects_[effect_id.c_str()];
  if (!effect) {
    effect = BuildEffect(doc, collada_effect);
    if (effect) effects_[effect_id.c_str()] = effect;
  }
  return effect;
}

// Builds an O3D effect from a COLLADA effect.  If a COLLADA-FX
// (Cg/HLSL) effect is present, it will be used and a programmable
// Effect generated.  If not, an attempt is made to use one of the fixed-
// function profiles if present (eg., Constant, Lambert).
// Parameters:
//   doc:                The FCollada document from which to import nodes.
//   collada_effect:     The COLLADA effect from which shaders will be taken.
// Returns:
//   the newly-created effect, or NULL or error.
Effect* Collada::BuildEffect(FCDocument* doc, FCDEffect* collada_effect) {
  if (!doc || !collada_effect) return NULL;
  // TODO(o3d): Remove all of this to be replaced by parsing Collada-FX
  //    and profile_O3D only.
  Effect* effect = NULL;
  FCDEffectProfileFX* profile_fx = FindProfileFX(collada_effect);
  if (profile_fx) {
    if (profile_fx->GetCodeCount() > 0) {
      FCDEffectCode* code = profile_fx->GetCode(0);
      String effect_string;
      FilePath file_path;
      if (code->GetType() == FCDEffectCode::CODE) {
        fstring code_string = code->GetCode();
        effect_string = WideToUTF8(code_string.c_str());
        unique_filename_counter_++;
        String file_name = "embedded-shader-" +
                           IntToString(unique_filename_counter_) + ".fx";
        file_path = FilePath(FILE_PATH_LITERAL("shaders"));
        file_path.Append(UTF8ToFilePath(file_name));
      } else if (code->GetType() == FCDEffectCode::INCLUDE) {
        fstring path = code->GetFilename();
        file_path = WideToFilePath(path.c_str());
        if (collada_zip_archive_ && !file_path.empty()) {
          // Make absolute path be relative to root of archive.
          if (file_path.value()[0] == FILE_PATH_LITERAL('/')) {
            file_path = FilePath(file_path.value().substr(1));
          }
          size_t effect_data_size;
          char *effect_data =
              collada_zip_archive_->GetFileData(FilePathToUTF8(file_path),
                                                &effect_data_size);
          if (effect_data) {
            effect_string = effect_data;
            free(effect_data);
          } else {
            O3D_ERROR(service_locator_)
                << "Unable to read effect data for effect '"
                << FilePathToUTF8(file_path) << "'";
            return NULL;
          }
        } else {
          FilePath temp_path = file_path;
          GetRelativePathIfPossible(base_path_, temp_path, &temp_path);
          file_util::ReadFileToString(temp_path, &effect_string);
        }
      }
      String collada_effect_name = WideToUTF8(
          collada_effect->GetName().c_str());
      effect = pack_->Create<Effect>();
      effect->set_name(collada_effect_name);

      ParamString* param = effect->CreateParam<ParamString>(
          O3D_STRING_CONSTANT("uri"));
      DCHECK(param != NULL);
      param->set_value(FilePathToUTF8(file_path));

      if (!effect->LoadFromFXString(effect_string)) {
        pack_->RemoveObject(effect);
        O3D_ERROR(service_locator_) << "Unable to load effect '"
                                    << FilePathToUTF8(file_path).c_str()
                                    << "'";
        return NULL;
      }
      if (options_.keep_original_data) {
        // Cache the original data by URI so we can recover it later.
        original_data_[file_path] = effect_string;
      }
    }
  } else {
    FCDExtra* extra = collada_effect->GetExtra();
    if (extra && extra->GetTypeCount() > 0) {
      FCDEType* type = extra->GetType(0);
      if (type) {
        FCDETechnique* technique = type->FindTechnique("NV_import");
        if (technique) {
          FCDENode* node = technique->FindChildNode("import");
          if (node) {
            FCDEAttribute* url_attrib = node->FindAttribute("url");
            if (url_attrib) {
              fstring url = url_attrib->GetValue();
              const FUFileManager* mgr = doc->GetFileManager();
              LOG_ASSERT(mgr != NULL);
              FUUri uri = mgr->GetCurrentUri();
              FUUri effect_uri = uri.Resolve(url_attrib->GetValue());
              fstring path = effect_uri.GetAbsolutePath();

              String collada_effect_name = WideToUTF8(
                  collada_effect->GetName().c_str());

              FilePath file_path = WideToFilePath(path.c_str());

              String effect_string;

              if (collada_zip_archive_ && !file_path.empty()) {
                // Make absolute path be relative to root of archive.
                if (file_path.value()[0] == FILE_PATH_LITERAL('/')) {
                  file_path = FilePath(file_path.value().substr(1));
                }
                // shader file can be extracted in memory from zip archive
                // so lets get the data and turn it into a string
                size_t effect_data_size;
                char *effect_data =
                    collada_zip_archive_->GetFileData(FilePathToUTF8(file_path),
                                                      &effect_data_size);
                if (effect_data) {
                  effect_string = effect_data;
                  free(effect_data);
                } else {
                  O3D_ERROR(service_locator_)
                      << "Unable to read effect data for effect '"
                      << FilePathToUTF8(file_path) << "'";
                  return NULL;
                }
              } else {
                file_util::ReadFileToString(file_path, &effect_string);
              }

              effect = pack_->Create<Effect>();
              effect->set_name(collada_effect_name);

              ParamString* param = effect->CreateParam<ParamString>(
                  O3D_STRING_CONSTANT("uri"));
              DCHECK(param != NULL);
              param->set_value(FilePathToUTF8(file_path));

              if (!effect->LoadFromFXString(effect_string)) {
                pack_->RemoveObject(effect);
                O3D_ERROR(service_locator_) << "Unable to load effect '"
                                            << FilePathToUTF8(file_path).c_str()
                                            << "'";
                return NULL;
              }
              if (options_.keep_original_data) {
                // Cache the original data by URI so we can recover it later.
                original_data_[file_path] = effect_string;
              }
            }
          }
        }
      }
    }
  }
  return effect;
}

// Gets a typed value from an FCollada state.  T is the type to get,
// state is the state from which to retrieve the value, and offset is
// the index into the state's data (in sizeof T objects) at which the
// value is located.
template <class T>
static T GetStateValue(FCDEffectPassState* state, size_t offset) {
  LOG_ASSERT(offset * sizeof(T) < state->GetDataSize());
  return *(reinterpret_cast<T*>(state->GetData()) + offset);
}

// Converts an FCollada blend type into an O3D state blending function.
// On error, an error string is logged.  "type" is the FCollada type to convert,
// and "collada" is the Collada importer used for logging errors.
static State::BlendingFunction ConvertBlendType(
    ServiceLocator* service_locator,
    FUDaePassStateBlendType::Type type) {
  switch (type) {
    case FUDaePassStateBlendType::ZERO:
      return State::BLENDFUNC_ZERO;
    case FUDaePassStateBlendType::ONE:
      return State::BLENDFUNC_ONE;
    case FUDaePassStateBlendType::SOURCE_COLOR:
      return State::BLENDFUNC_SOURCE_COLOR;
    case FUDaePassStateBlendType::ONE_MINUS_SOURCE_COLOR:
      return State::BLENDFUNC_INVERSE_SOURCE_COLOR;
    case FUDaePassStateBlendType::SOURCE_ALPHA:
      return State::BLENDFUNC_SOURCE_ALPHA;
    case FUDaePassStateBlendType::ONE_MINUS_SOURCE_ALPHA:
      return State::BLENDFUNC_INVERSE_SOURCE_ALPHA;
    case FUDaePassStateBlendType::DESTINATION_ALPHA:
      return State::BLENDFUNC_DESTINATION_ALPHA;
    case FUDaePassStateBlendType::ONE_MINUS_DESTINATION_ALPHA:
      return State::BLENDFUNC_INVERSE_DESTINATION_ALPHA;
    case FUDaePassStateBlendType::DESTINATION_COLOR:
      return State::BLENDFUNC_DESTINATION_COLOR;
    case FUDaePassStateBlendType::ONE_MINUS_DESTINATION_COLOR:
      return State::BLENDFUNC_INVERSE_DESTINATION_COLOR;
    case FUDaePassStateBlendType::SOURCE_ALPHA_SATURATE:
      return State::BLENDFUNC_SOURCE_ALPHA_SATUTRATE;
    default:
      O3D_ERROR(service_locator) << "Invalid blend type";
      return State::BLENDFUNC_ONE;
  }
}

// Converts an FCollada blend equation into an O3D state blending
// equation.  On error, an error string is logged.  "equation" is the
// FCollada equation to convert, and "collada" is the Collada importer
// used for logging errors.
static State::BlendingEquation ConvertBlendEquation(
    ServiceLocator* service_locator,
    FUDaePassStateBlendEquation::Equation equation) {
  switch (equation) {
    case FUDaePassStateBlendEquation::ADD:
      return State::BLEND_ADD;
    case FUDaePassStateBlendEquation::SUBTRACT:
      return State::BLEND_SUBTRACT;
    case FUDaePassStateBlendEquation::REVERSE_SUBTRACT:
      return State::BLEND_REVERSE_SUBTRACT;
    case FUDaePassStateBlendEquation::MIN:
      return State::BLEND_MIN;
    case FUDaePassStateBlendEquation::MAX:
      return State::BLEND_MAX;
    default:
      O3D_ERROR(service_locator) << "Invalid blend equation";
      return State::BLEND_ADD;
  }
}

// Converts an FCollada comparison function into an O3D state blending
// equation.  On error, an error string is logged.  "function" is the
// FCollada comparison function to convert, and "collada" is the Collada
// importer used for logging errors.
static State::Comparison ConvertComparisonFunction(
    ServiceLocator* service_locator,
    FUDaePassStateFunction::Function function) {
  switch (function) {
    case FUDaePassStateFunction::NEVER:
      return State::CMP_NEVER;
    case FUDaePassStateFunction::LESS:
      return State::CMP_LESS;
    case FUDaePassStateFunction::LESS_EQUAL:
      return State::CMP_LEQUAL;
    case FUDaePassStateFunction::EQUAL:
      return State::CMP_EQUAL;
    case FUDaePassStateFunction::GREATER:
      return State::CMP_GREATER;
    case FUDaePassStateFunction::NOT_EQUAL:
      return State::CMP_NOTEQUAL;
    case FUDaePassStateFunction::GREATER_EQUAL:
      return State::CMP_GEQUAL;
    case FUDaePassStateFunction::ALWAYS:
      return State::CMP_ALWAYS;
    default:
      O3D_ERROR(service_locator) << "Invalid comparison function";
      return State::CMP_NEVER;
  }
}

// Converts an FCollada polygon fill mode into an O3D fill mode.
// On error, an error string is logged.  "mode" is the FCollada fill
// mode to convert, and "collada" is the Collada importer used for
// logging errors.
static State::Fill ConvertFillMode(ServiceLocator* service_locator,
                                   FUDaePassStatePolygonMode::Mode mode) {
  switch (mode) {
    case FUDaePassStatePolygonMode::POINT:
      return State::POINT;
    case FUDaePassStatePolygonMode::LINE:
      return State::WIREFRAME;
    case FUDaePassStatePolygonMode::FILL:
      return State::SOLID;
    default:
      O3D_ERROR(service_locator) << "Invalid polygon fill mode";
      return State::SOLID;
  }
}

// Converts an FCollada stencil operation into an O3D stencil operation.
// On error, an error string is logged.  "operation" is the FCollada operation
// to convert, and "collada" is the Collada importer used for logging errors.
static State::StencilOperation ConvertStencilOp(
    ServiceLocator* service_locator,
    FUDaePassStateStencilOperation::Operation operation) {
  switch (operation) {
    case FUDaePassStateStencilOperation::KEEP:
      return State::STENCIL_KEEP;
    case FUDaePassStateStencilOperation::ZERO:
      return State::STENCIL_ZERO;
    case FUDaePassStateStencilOperation::REPLACE:
      return State::STENCIL_REPLACE;
    case FUDaePassStateStencilOperation::INCREMENT:
      return State::STENCIL_INCREMENT_SATURATE;
    case FUDaePassStateStencilOperation::DECREMENT:
      return State::STENCIL_DECREMENT_SATURATE;
    case FUDaePassStateStencilOperation::INVERT:
      return State::STENCIL_INVERT;
    case FUDaePassStateStencilOperation::INCREMENT_WRAP:
      return State::STENCIL_INCREMENT;
    case FUDaePassStateStencilOperation::DECREMENT_WRAP:
      return State::STENCIL_DECREMENT;
    default:
      O3D_ERROR(service_locator) << "Invalid stencil operation";
      return State::STENCIL_KEEP;
  }
}

// Updates the O3D state object's cull mode from the OpenGL-style
// cull modes used by COLLADA-FX sections.  "state" is the O3D state
// on which to set the cull mode.  If "cull_front_" is true, the system is
// culling front-facing polygons, otherwise it's culling back-facing.  If
// "front_cw_" is true, polygons with clockwise winding are considered
// front-facing, otherwise counter-clockwise winding is considered
// front-facing.  if "cull_enabled_" is false, culling is disabled.
void Collada::UpdateCullingState(State* state) {
  ParamInteger* face = state->GetStateParam<ParamInteger>(
      State::kCullModeParamName);
  if (cull_front_ ^ front_cw_) {
    face->set_value(cull_enabled_ ? State::CULL_CCW : State::CULL_NONE);
  } else {
    face->set_value(cull_enabled_ ? State::CULL_CW : State::CULL_NONE);
  }
}

// Sets a boolean state param in an O3D state object.  "state" is the
// state object, "name" is the name of the state param to set, and "value"
// is the boolean value.  If the state param does not exist, or is not
// of the correct type, an assert is logged.
static void SetBoolState(State* state, const char* name, bool value) {
  ParamBoolean* param = state->GetStateParam<ParamBoolean>(name);
  LOG_ASSERT(param);
  param->set_value(value);
}

// Sets a floating-point state param in an O3D state object.
// "state" is the state object, "name" is the name of the state param
// to set, and "value" is the floating-point value.  If the state
// param does not exist, or is not of the correct type, an assert
// is logged.
static void SetFloatState(State* state, const char* name, float value) {
  ParamFloat* param = state->GetStateParam<ParamFloat>(name);
  LOG_ASSERT(param);
  param->set_value(value);
}

// Sets a Float4 state param in an O3D state object.  "state" is
// the state object, "name" is the name of the state param to set, and
// "value" is the Float4 value.  If the state param does not exist, or
// is not of the correct type, an assert is logged.
static void SetFloat4State(State* state,
                           const char* name,
                           const Float4& value) {
  ParamFloat4* param = state->GetStateParam<ParamFloat4>(name);
  LOG_ASSERT(param);
  param->set_value(value);
}

// Sets a integer state param in an O3D state object.  "state" is
// the state object, "name" is the name of the state param to set, and
// "value" is the Float4 value.  If the state param does not exist, or
// is not of the correct type, an assert is logged.
static void SetIntState(State* state, const char* name, int value) {
  ParamInteger* param = state->GetStateParam<ParamInteger>(name);
  LOG_ASSERT(param);
  param->set_value(value);
}

// Sets the stencil state params for polygons with clockwise
// winding.  "state" is the O3D state object in which to set
// state, "fail", "zfail", and "zpass" are the corresponding stencil
// settings.
static void SetCWStencilSettings(State* state,
                                 State::StencilOperation fail,
                                 State::StencilOperation zfail,
                                 State::StencilOperation zpass) {
  SetIntState(state, State::kStencilFailOperationParamName, fail);
  SetIntState(state, State::kStencilZFailOperationParamName, zfail);
  SetIntState(state, State::kStencilPassOperationParamName, zpass);
}

// Sets the stencil state params for polygons with counter-clockwise
// winding.  "state" is the O3D state object in which to set
// state, "fail", "zfail", and "zpass" are the corresponding stencil
// settings.
static void SetCCWStencilSettings(State* state,
                                  State::StencilOperation fail,
                                  State::StencilOperation zfail,
                                  State::StencilOperation zpass) {
  SetIntState(state, State::kCCWStencilFailOperationParamName, fail);
  SetIntState(state, State::kCCWStencilZFailOperationParamName, zfail);
  SetIntState(state, State::kCCWStencilPassOperationParamName, zpass);
}

// Sets the stencil state params for polygons with the given winding
// order.  "state" is the O3D state object in which to set
// state, "fail", "zfail", and "zpass" are the corresponding stencil
// settings.  If "cw" is true, clockwise winding settings will be set,
// otherwise counter-clockwise winding settings are set.
static void SetStencilSettings(State* state,
                               bool cw,
                               State::StencilOperation fail,
                               State::StencilOperation zfail,
                               State::StencilOperation zpass) {
  if (cw) {
    SetCWStencilSettings(state, fail, zfail, zpass);
  } else {
    SetCCWStencilSettings(state, fail, zfail, zpass);
  }
}

// Adds the appropriate O3D state params to the given state object
// corresponding to a given FCollada Pass State.  If unsupported or
// invalid states are specified, an error message is set.
void Collada::AddRenderState(FCDEffectPassState* pass_state, State* state) {
  switch (pass_state->GetType()) {
    case FUDaePassState::ALPHA_FUNC: {
      State::Comparison function = ConvertComparisonFunction(
          service_locator_,
          GetStateValue<FUDaePassStateFunction::Function>(pass_state, 0));
      float value = GetStateValue<float>(pass_state, 1);
      SetIntState(state, State::kAlphaComparisonFunctionParamName, function);
      SetFloatState(state, State::kAlphaReferenceParamName, value);
      break;
    }
    case FUDaePassState::BLEND_FUNC: {
      State::BlendingFunction src = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 0));
      State::BlendingFunction dest = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 1));
      SetIntState(state, State::kSourceBlendFunctionParamName, src);
      SetIntState(state, State::kDestinationBlendFunctionParamName, dest);
      SetBoolState(state, State::kSeparateAlphaBlendEnableParamName, false);
      break;
    }
    case FUDaePassState::BLEND_FUNC_SEPARATE: {
      State::BlendingFunction src_rgb = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 0));
      State::BlendingFunction dest_rgb = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 1));
      State::BlendingFunction src_alpha = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 2));
      State::BlendingFunction dest_alpha = ConvertBlendType(
          service_locator_,
          GetStateValue<FUDaePassStateBlendType::Type>(pass_state, 3));
      SetIntState(state, State::kSourceBlendFunctionParamName, src_rgb);
      SetIntState(state, State::kDestinationBlendFunctionParamName, dest_rgb);
      SetIntState(state, State::kSourceBlendAlphaFunctionParamName, src_alpha);
      SetIntState(state, State::kDestinationBlendAlphaFunctionParamName,
                  dest_alpha);
      SetBoolState(state, State::kSeparateAlphaBlendEnableParamName, true);
      break;
    }
    case FUDaePassState::BLEND_EQUATION: {
      State::BlendingEquation value = ConvertBlendEquation(
          service_locator_,
          GetStateValue<FUDaePassStateBlendEquation::Equation>(pass_state, 0));
      SetIntState(state, State::kBlendEquationParamName, value);
      SetIntState(state, State::kBlendAlphaEquationParamName, value);
      break;
    }
    case FUDaePassState::BLEND_EQUATION_SEPARATE: {
      State::BlendingEquation rgb = ConvertBlendEquation(
          service_locator_,
          GetStateValue<FUDaePassStateBlendEquation::Equation>(pass_state, 0));
      State::BlendingEquation alpha = ConvertBlendEquation(
          service_locator_,
          GetStateValue<FUDaePassStateBlendEquation::Equation>(pass_state, 1));
      SetIntState(state, State::kBlendEquationParamName, rgb);
      SetIntState(state, State::kBlendAlphaEquationParamName, alpha);
      break;
    }
    case FUDaePassState::CULL_FACE: {
      FUDaePassStateFaceType::Type culled_faces =
          GetStateValue<FUDaePassStateFaceType::Type>(pass_state, 0);
      switch (culled_faces) {
        case FUDaePassStateFaceType::FRONT: {
          cull_front_ = true;
          break;
        }
        case FUDaePassStateFaceType::BACK: {
          cull_front_ = false;
          break;
        }
        case FUDaePassStateFaceType::FRONT_AND_BACK: {
          O3D_ERROR(service_locator_)
              << "FRONT_AND_BACK culling is unsupported";
          break;
        }
      }
      UpdateCullingState(state);
      break;
    }
    case FUDaePassState::DEPTH_FUNC: {
      State::Comparison function = ConvertComparisonFunction(
          service_locator_,
          GetStateValue<FUDaePassStateFunction::Function>(pass_state, 0));
      SetIntState(state, State::kZComparisonFunctionParamName, function);
      break;
    }
    case FUDaePassState::FRONT_FACE: {
      FUDaePassStateFrontFaceType::Type type =
          GetStateValue<FUDaePassStateFrontFaceType::Type>(pass_state, 0);
      front_cw_ = type == FUDaePassStateFrontFaceType::CLOCKWISE;
      break;
    }
    case FUDaePassState::POLYGON_MODE: {
      FUDaePassStateFaceType::Type face =
          GetStateValue<FUDaePassStateFaceType::Type>(pass_state, 0);
      State::Fill mode = ConvertFillMode(
          service_locator_,
          GetStateValue<FUDaePassStatePolygonMode::Mode>(pass_state, 1));
      if (face != FUDaePassStateFaceType::FRONT_AND_BACK) {
        O3D_ERROR(service_locator_)
            << "Separate polygon fill modes are unsupported";
      }
      SetIntState(state, State::kFillModeParamName, mode);
      break;
    }
    case FUDaePassState::STENCIL_FUNC: {
      State::Comparison func = ConvertComparisonFunction(
          service_locator_,
          GetStateValue<FUDaePassStateFunction::Function>(pass_state, 0));
      uint8 ref = GetStateValue<uint8>(pass_state, 4);
      uint8 mask = GetStateValue<uint8>(pass_state, 5);
      SetIntState(state, State::kStencilComparisonFunctionParamName, func);
      SetIntState(state, State::kStencilReferenceParamName, ref);
      SetIntState(state, State::kStencilMaskParamName, mask);
      SetBoolState(state, State::kTwoSidedStencilEnableParamName, false);
      break;
    }
    case FUDaePassState::STENCIL_FUNC_SEPARATE: {
      State::Comparison front = ConvertComparisonFunction(
          service_locator_,
          GetStateValue<FUDaePassStateFunction::Function>(pass_state, 0));
      State::Comparison back = ConvertComparisonFunction(
          service_locator_,
          GetStateValue<FUDaePassStateFunction::Function>(pass_state, 1));
      uint8 ref = GetStateValue<uint8>(pass_state, 8);
      uint8 mask = GetStateValue<uint8>(pass_state, 9);
      SetIntState(state, State::kStencilComparisonFunctionParamName, front);
      SetIntState(state, State::kCCWStencilComparisonFunctionParamName, back);
      SetIntState(state, State::kStencilReferenceParamName, ref);
      SetIntState(state, State::kStencilMaskParamName, mask);
      SetBoolState(state, State::kTwoSidedStencilEnableParamName, true);
      break;
    }
    case FUDaePassState::STENCIL_OP: {
      State::StencilOperation fail = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   0));
      State::StencilOperation zfail = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   1));
      State::StencilOperation zpass = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   2));
      SetStencilSettings(state, front_cw_, fail, zfail, zpass);
      SetBoolState(state, State::kTwoSidedStencilEnableParamName, false);
      break;
    }
    case FUDaePassState::STENCIL_OP_SEPARATE: {
      FUDaePassStateFaceType::Type type =
          GetStateValue<FUDaePassStateFaceType::Type>(pass_state, 0);
      State::StencilOperation fail = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   0));
      State::StencilOperation zfail = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   1));
      State::StencilOperation zpass = ConvertStencilOp(
          service_locator_,
          GetStateValue<FUDaePassStateStencilOperation::Operation>(pass_state,
                                                                   2));
      switch (type) {
        case FUDaePassStateFaceType::FRONT:
          SetStencilSettings(state, front_cw_, fail, zfail, zpass);
          SetBoolState(state, State::kTwoSidedStencilEnableParamName, true);
          break;
        case FUDaePassStateFaceType::BACK:
          SetStencilSettings(state, !front_cw_, fail, zfail, zpass);
          SetBoolState(state, State::kTwoSidedStencilEnableParamName, true);
          break;
        case FUDaePassStateFaceType::FRONT_AND_BACK:
          SetStencilSettings(state, front_cw_, fail, zfail, zpass);
          SetStencilSettings(state, !front_cw_, fail, zfail, zpass);
          SetBoolState(state, State::kTwoSidedStencilEnableParamName, false);
          break;
        default:
          O3D_ERROR(service_locator_)
              << "Unknown polygon face mode in STENCIL_OP_SEPARATE";
          break;
      }
      break;
    }
    case FUDaePassState::STENCIL_MASK: {
      uint32 mask = GetStateValue<uint8>(pass_state, 0);
      SetIntState(state, State::kStencilWriteMaskParamName, mask);
      break;
    }
    case FUDaePassState::STENCIL_MASK_SEPARATE: {
      O3D_ERROR(service_locator_) << "Separate stencil mask is unsupported";
      break;
    }
    case FUDaePassState::COLOR_MASK: {
      bool red = GetStateValue<bool>(pass_state, 0);
      bool green = GetStateValue<bool>(pass_state, 1);
      bool blue = GetStateValue<bool>(pass_state, 2);
      bool alpha = GetStateValue<bool>(pass_state, 3);
      int mask = 0x0;
      if (red) mask |= 0x1;
      if (green) mask |= 0x2;
      if (blue) mask |= 0x4;
      if (alpha) mask |= 0x8;
      SetIntState(state, State::kColorWriteEnableParamName, mask);
      break;
    }
    case FUDaePassState::DEPTH_MASK: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kZWriteEnableParamName, value);
      break;
    }
    case FUDaePassState::POINT_SIZE: {
      float value = GetStateValue<float>(pass_state, 0);
      SetFloatState(state, State::kPointSizeParamName, value);
      break;
    }
    case FUDaePassState::POLYGON_OFFSET: {
      float value1 = GetStateValue<float>(pass_state, 0);
      float value2 = GetStateValue<float>(pass_state, 1);
      SetFloatState(state, State::kPolygonOffset1ParamName, value1);
      SetFloatState(state, State::kPolygonOffset2ParamName, value2);
      break;
    }
    case FUDaePassState::BLEND_COLOR: {
      FMVector4 value = GetStateValue<FMVector4>(pass_state, 0);
      Float4 v(value.x, value.y, value.z, value.w);
      SetFloat4State(state, State::kPolygonOffset1ParamName, v);
      break;
    }
    case FUDaePassState::ALPHA_TEST_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kAlphaTestEnableParamName, value);
      break;
    }
    case FUDaePassState::BLEND_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kAlphaBlendEnableParamName, value);
      break;
    }
    case FUDaePassState::CULL_FACE_ENABLE: {
      cull_enabled_ = GetStateValue<bool>(pass_state, 0);
      UpdateCullingState(state);
      break;
    }
    case FUDaePassState::DEPTH_TEST_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kZEnableParamName, value);
      break;
    }
    case FUDaePassState::DITHER_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kDitherEnableParamName, value);
      break;
    }
    case FUDaePassState::LINE_SMOOTH_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kLineSmoothEnableParamName, value);
      break;
    }
    case FUDaePassState::STENCIL_TEST_ENABLE: {
      bool value = GetStateValue<bool>(pass_state, 0);
      SetBoolState(state, State::kStencilEnableParamName, value);
      break;
    }
  }
}

// Converts an FCollada texture sampler wrap mode to an O3D Sampler
// AddressMode.
static Sampler::AddressMode ConvertSamplerAddressMode(
    FUDaeTextureWrapMode::WrapMode wrap_mode) {
  switch (wrap_mode) {
    case FUDaeTextureWrapMode::WRAP:
      return Sampler::WRAP;
    case FUDaeTextureWrapMode::MIRROR:
      return Sampler::MIRROR;
    case FUDaeTextureWrapMode::CLAMP:
      return Sampler::CLAMP;
    case FUDaeTextureWrapMode::BORDER:
      return Sampler::BORDER;
    default:
      return Sampler::WRAP;
  }
}

// Converts an FCollada filter func to an O3D Sampler FilterType.
// Since the Collada filter spec allows both GL-style combo mag/min filters,
// and DX-style (separate min/mag/mip filters), this function extracts
// only the first (min) part of a GL-style filter.
static Sampler::FilterType ConvertFilterType(
    FUDaeTextureFilterFunction::FilterFunction filter_function,
    bool allow_none) {
  switch (filter_function) {
    case FUDaeTextureFilterFunction::NEAREST:
    case FUDaeTextureFilterFunction::NEAREST_MIPMAP_NEAREST:
    case FUDaeTextureFilterFunction::NEAREST_MIPMAP_LINEAR:
      return Sampler::POINT;
    case FUDaeTextureFilterFunction::LINEAR:
    case FUDaeTextureFilterFunction::LINEAR_MIPMAP_NEAREST:
    case FUDaeTextureFilterFunction::LINEAR_MIPMAP_LINEAR:
      return Sampler::LINEAR;
      // TODO(o3d): Once FCollada supports COLLADA v1.5, turn this on:
      // case FUDaeTextureFilterFunction::ANISOTROPIC:
      //   return Sampler::ANISOTROPIC;
    case FUDaeTextureFilterFunction::NONE:
      return allow_none ? Sampler::NONE : Sampler::LINEAR;
    default:
      return Sampler::LINEAR;
  }
}


// Retrieves the mipmap part of a GL-style filter function.
// If no mipmap part is specified, it is assumed to be POINT.
static Sampler::FilterType ConvertMipmapFilter(
    FUDaeTextureFilterFunction::FilterFunction filter_function) {
  switch (filter_function) {
    case FUDaeTextureFilterFunction::NEAREST_MIPMAP_NEAREST:
    case FUDaeTextureFilterFunction::LINEAR_MIPMAP_NEAREST:
    case FUDaeTextureFilterFunction::UNKNOWN:
      return Sampler::POINT;
    case FUDaeTextureFilterFunction::NEAREST_MIPMAP_LINEAR:
    case FUDaeTextureFilterFunction::LINEAR_MIPMAP_LINEAR:
      return Sampler::LINEAR;
    default:
      return Sampler::NONE;
  }
}

// Sets the texture sampler states on an O3D sampler from the
// settings found in an FCollada sampler.
// Parameters:
//   effect_sampler:       The FCollada sampler.
//   o3d_sampler:          The O3D sampler object.
void Collada::SetSamplerStates(FCDEffectParameterSampler* effect_sampler,
                               Sampler* o3d_sampler) {
  FUDaeTextureWrapMode::WrapMode wrap_s = effect_sampler->GetWrapS();
  FUDaeTextureWrapMode::WrapMode wrap_t = effect_sampler->GetWrapT();
  Texture *texture = o3d_sampler->texture();
  if (texture && texture->IsA(TextureCUBE::GetApparentClass())) {
    // Our default is WRAP, but cube maps should use CLAMP
    if (wrap_s == FUDaeTextureWrapMode::UNKNOWN)
      wrap_s = FUDaeTextureWrapMode::CLAMP;
    if (wrap_t == FUDaeTextureWrapMode::UNKNOWN)
      wrap_t = FUDaeTextureWrapMode::CLAMP;
  }

  FUDaeTextureFilterFunction::FilterFunction min_filter =
      effect_sampler->GetMinFilter();
  FUDaeTextureFilterFunction::FilterFunction mag_filter =
      effect_sampler->GetMagFilter();

  FUDaeTextureFilterFunction::FilterFunction mip_filter =
      effect_sampler->GetMipFilter();
  // TODO(o3d): Once FCollada supports COLLADA v1.5, turn this on:
  // int max_anisotropy = effect_sampler->GetMaxAnisotropy();

  o3d_sampler->set_address_mode_u(ConvertSamplerAddressMode(wrap_s));
  o3d_sampler->set_address_mode_v(ConvertSamplerAddressMode(wrap_t));

  // The Collada spec allows for both DX-style and GL-style specification
  // of texture filtering modes.  In DX-style, Min, Mag and Mip filters
  // are specified separately, and may be Linear, Point or None.
  // In GL-style, only Min and Mag are specified, with the Mip filter
  // encoded as a combo setting in the Min filter.  E.g.,
  // LinearMipmapLinear  => Min Linear, Mip Linear,
  // LinearMipmapNearest => Min Linear, Mip Point,
  // Linear              => Min Linear, Mip None (no mipmapping).

  // In order to sort this out, if the Mip filter is "unknown" (missing),
  // we assume GL-style specification, and extract the Mip setting from
  // the latter part of the GL-style Min setting.  If the Mip filter is
  // specified, we assume a DX-style specification, and the three
  // components are assigned separately.  Any GL-style combo
  // modes used in DX mode are ignored (only the first part is used).

  o3d_sampler->set_min_filter(ConvertFilterType(min_filter, false));
  o3d_sampler->set_mag_filter(ConvertFilterType(mag_filter, false));

  // If the mip filter is set to "UNKNOWN", we assume it's a GL-style
  // mode, and use the 2nd part of the Min filter for the mip type.  Otherwise,
  // we use the first part.
  if (mip_filter == FUDaeTextureFilterFunction::UNKNOWN) {
    o3d_sampler->set_mip_filter(ConvertMipmapFilter(min_filter));
  } else {
    o3d_sampler->set_mip_filter(ConvertFilterType(mip_filter, true));
  }

  // TODO(o3d): Once FCollada supports COLLADA v1.5, turn this on:
  // o3d_sampler->set_max_anisotropy(max_anisotropy);
}

// Sets the value of a Param on the given ParamObject from an FCollada
// standard-profile effect parameter.  If the FCollada parameter
// contains a texture, the sampler_param_name and channel is used to set
// a Sampler Param in o3d from the surface.  If not, the
// color_param_name is used to create set a vector Param value.
// Parameters:
//   effect_standard:      The fixed-function FCollada effect from which
//                         to retrieve the parameter value.
//   param_object:         The ParamObject on which parameters will be set.
//   color_param_mame:     The name of the param to set, if a vector.
//   sampler_param_mame:   The name of the param to set, if a texture sampler.
//   color_param:          The FCollada parameter from which to retrieve the
//                         new value.
//   channel:              The texture channel to use (if any).
void Collada::SetParamFromStandardEffectParam(
    FCDEffectStandard* effect_standard,
    ParamObject* param_object,
    const char* color_param_name,
    const char* sampler_param_name,
    FCDEffectParameter* color_param,
    int channel) {
  if (effect_standard->GetTextureCount(channel) > 0) {
    FCDTexture* texture = effect_standard->GetTexture(channel, 0);
    FCDEffectParameterSampler* sampler = texture->GetSampler();
    SetParamFromFCEffectParam(param_object, sampler_param_name, sampler);
  } else if (color_param) {
    SetParamFromFCEffectParam(param_object, color_param_name, color_param);
  }
}

// Sets the values of a ParamObject parameters from a given FCollada material
// node. If a corresponding ParamObject parameter is not found, the FCollada
// parameter is ignored.
// TODO(o3d): Should we ignore params not found? Maybe the user wants those for
//    things other than rendering.
// Parameters:
//   material:  The FCollada material node from which to retrieve values.
//   param_object:     The ParamObject on which parameters will be set.
void Collada::SetParamsFromMaterial(FCDMaterial* material,
                                    ParamObject* param_object) {
  size_t pcount = material->GetEffectParameterCount();
  // TODO(o3d):  This test (for determining if we used the
  // programmable profile or the fixed-func profile) is not very robust.
  // Remove this once the Material changes are in.
  if (pcount > 0) {
    for (size_t i = 0; i < pcount; ++i) {
      FCDEffectParameter* p = material->GetEffectParameter(i);
      LOG_ASSERT(p);
      String param_name(p->GetReference());
      // Check for an effect binding
      FCDEffect* effect = material->GetEffect();
      FCDEffectProfileFX* profile_fx = FindProfileFX(material->GetEffect());
      if (profile_fx) {
        FCDEffectTechnique* technique = profile_fx->GetTechnique(0);
        if (technique->GetPassCount() > 0) {
          FCDEffectPass* pass = technique->GetPass(0);
          for (size_t j = 0; j < pass->GetShaderCount(); ++j) {
            FCDEffectPassShader* shader = pass->GetShader(j);
            FCDEffectPassBind* bind =
                shader->FindBindingReference(p->GetReference());
            if (bind) {
              param_name = *bind->symbol;
              break;
            }
          }
        }
      }
      SetParamFromFCEffectParam(param_object, param_name, p);
    }
  } else {
    FCDEffect* effect = material->GetEffect();
    FCDEffectStandard* effect_standard;
    if (effect && (effect_standard = static_cast<FCDEffectStandard *>(
        effect->FindProfile(FUDaeProfileType::COMMON))) != NULL) {
      SetParamFromStandardEffectParam(effect_standard,
                                      param_object,
                                      kMaterialParamNameEmissive,
                                      kMaterialParamNameEmissiveSampler,
                                      effect_standard->GetEmissionColorParam(),
                                      FUDaeTextureChannel::EMISSION);
      SetParamFromStandardEffectParam(effect_standard,
                                      param_object,
                                      kMaterialParamNameAmbient,
                                      kMaterialParamNameAmbientSampler,
                                      effect_standard->GetAmbientColorParam(),
                                      FUDaeTextureChannel::AMBIENT);
      SetParamFromStandardEffectParam(effect_standard,
                                      param_object,
                                      kMaterialParamNameDiffuse,
                                      kMaterialParamNameDiffuseSampler,
                                      effect_standard->GetDiffuseColorParam(),
                                      FUDaeTextureChannel::DIFFUSE);
      SetParamFromStandardEffectParam(effect_standard,
                                      param_object,
                                      kMaterialParamNameSpecular,
                                      kMaterialParamNameSpecularSampler,
                                      effect_standard->GetSpecularColorParam(),
                                      FUDaeTextureChannel::SPECULAR);
      SetParamFromStandardEffectParam(effect_standard,
                                      param_object,
                                      NULL,
                                      kMaterialParamNameBumpSampler,
                                      NULL,
                                      FUDaeTextureChannel::BUMP);
      SetParamFromFCEffectParam(param_object,
                                kMaterialParamNameShininess,
                                effect_standard->GetShininessParam());
      SetParamFromFCEffectParam(param_object,
                                kMaterialParamNameSpecularFactor,
                                effect_standard->GetSpecularFactorParam());
    }
  }
}
}  // namespace o3d
