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


// This file declares functions for importing COLLADA files into O3D.

#ifndef O3D_IMPORT_CROSS_COLLADA_H_
#define O3D_IMPORT_CROSS_COLLADA_H_

#include <map>
#include <string>
#include <vector>
#include "base/file_path.h"
#include "core/cross/param.h"
#include "core/cross/types.h"

class FCDocument;
class FCDAnimated;
class FCDCamera;
class FCDSceneNode;
class FCDGeometry;
class FCDGeometryInstance;
class FCDControllerInstance;
class FCDMaterial;
class FCDEffect;
class FCDEffectStandard;
class FCDEffectParameter;
class FCDEffectParameterSampler;
class FCDEffectParameterSurface;
class FCDEffectPassState;
class FCDImage;
class FCDTMatrix;
class FCDTTranslation;
class FCDTRotation;
class FCDTScale;

class FilePath;

namespace o3d {

class ColladaZipArchive;
class Effect;
class IErrorStatus;
class Material;
class Pack;
class ParamObject;
class Sampler;
class ShaderBuilderHelper;
class Shape;
class State;
class Texture;
class Transform;
class TranslationMap;

// This class keeps an association between a Collada node instance and a
// transform.
// This takes ownership of its children NodeInstances.
class NodeInstance {
 public:
  typedef std::vector<NodeInstance *> NodeInstanceList;

  explicit NodeInstance(FCDSceneNode *node) : node_(node), transform_(NULL) {}
  ~NodeInstance() {
    for (unsigned int i = 0; i < children_.size(); ++i) {
      delete children_[i];
    }
  }

  // Gets the Collada node associated with this node instance.
  FCDSceneNode *node() const { return node_; }

  // Gets the Transform associated with this node instance.
  Transform *transform() const { return transform_; }

  // Sets the Transform associated with this node instance.
  void set_transform(Transform *transform) { transform_ = transform; }

  // Gets the list of this node instance's children.
  NodeInstanceList &children() { return children_; }

  // Finds the NodeInstance representing a scene node in the direct children of
  // this NodeInstance.
  NodeInstance *FindNodeShallow(FCDSceneNode *node) {
    for (unsigned int i = 0; i < children_.size(); ++i) {
      NodeInstance *child = children_[i];
      if (child->node() == node) return child;
    }
    return NULL;
  }

  // Finds the NodeInstance representing a scene node in the sub-tree starting
  // at this NodeInstance.
  NodeInstance *FindNodeInTree(FCDSceneNode *node);

 private:
  FCDSceneNode *node_;
  Transform *transform_;
  std::vector<NodeInstance *> children_;
};

class Collada {
 public:
  struct Options {
    Options()
        : generate_mipmaps(true),
          condition_document(false),
          keep_original_data(false),
          up_axis(0.0f, 0.0f, 0.0f),
          base_path(FilePath::kCurrentDirectory) {}
    // Whether or not to generate mip-maps on the textures we load.
    bool generate_mipmaps;

    // Whether or not to retain the original form for textures for later
    // access by filename.
    bool keep_original_data;

    // Whether or not to condition documents for o3d as part of
    // loading them.
    bool condition_document;

    // What the up-axis of the imported geometry should be.
    Vector3 up_axis;

    // The base path to use for determining the relative paths for
    // asset URIs.
    FilePath base_path;
  };

  // Collada Param Names.
  // TODO(gman): Remove these as we switch this stuff to JSON
  static const char* kLightingTypeParamName;

  // Lighitng Types.
  // TODO(gman): Remove these as we switch this stuff to JSON
  static const char* kLightingTypeConstant;
  static const char* kLightingTypePhong;
  static const char* kLightingTypeBlinn;
  static const char* kLightingTypeLambert;
  static const char* kLightingTypeUnknown;

  // Material Param Names.
  static const char* kMaterialParamNameEmissive;
  static const char* kMaterialParamNameAmbient;
  static const char* kMaterialParamNameDiffuse;
  static const char* kMaterialParamNameSpecular;
  static const char* kMaterialParamNameShininess;
  static const char* kMaterialParamNameSpecularFactor;
  static const char* kMaterialParamNameEmissiveSampler;
  static const char* kMaterialParamNameAmbientSampler;
  static const char* kMaterialParamNameDiffuseSampler;
  static const char* kMaterialParamNameSpecularSampler;
  static const char* kMaterialParamNameBumpSampler;

  // Use this if you need access to data after the import (as the
  // converter does).
  Collada(Pack* pack, const Options& options);
  virtual ~Collada();

  // Imports the given COLLADA file or ZIP file into the given scene.
  // This is the external interface to o3d.
  // Parameters:
  //   pack:      The pack into which the scene objects will be placed.
  //   filename:  The COLLADA or ZIPped COLLADA file to import.
  //   parent:    The parent node under which the imported nodes will be placed.
  //              If NULL, nodes will be placed under the client's root.
  //   animation_input: The counter parameter used to control transform
  //              animation in the collada file.
  //   options:   The Options structure (see above) that describes any
  //              desired options.
  // Returns true on success.
  static bool Import(Pack* pack,
                     const FilePath& filename,
                     Transform* parent,
                     ParamFloat* animation_input,
                     const Options& options);

  // Same thing but with String filename.
  static bool Import(Pack* pack,
                     const String& filename,
                     Transform* parent,
                     ParamFloat* animation_input,
                     const Options& options);

  // Imports the given COLLADA file or ZIP file into the pack given to
  // the constructor.
  bool ImportFile(const FilePath& filename, Transform* parent,
                  ParamFloat* animation_input);

  // Access to the filenames of the original data for texture and
  // sound assets imported when ImportFile was called.  These will
  // only return results after an import if the keep_original_data
  // option was set to true when the Collada object was created.
  std::vector<FilePath> GetOriginalDataFilenames() const;
  const std::string& GetOriginalData(const FilePath& filename) const;

 private:
  // Imports the given ZIP file into the given pack.
  bool ImportZIP(const FilePath& filename, Transform* parent,
                 ParamFloat* animation_input);

  // Imports the given COLLADA file (.DAE) into the current pack.
  bool ImportDAE(const FilePath& filename,
                 Transform* parent,
                 ParamFloat* animation_input);

  // Imports the given FCDocument (already loaded) into the current pack.
  bool ImportDAEDocument(FCDocument* doc,
                         bool fc_status,
                         Transform* parent,
                         ParamFloat* animation_input);

  // Creates the instance tree corresponding to the collada scene node DAG.
  // A separate NodeInstance is created every time a particular node is
  // traversed. The caller must destroy the returned NodeInstance.
  static NodeInstance *CreateInstanceTree(FCDSceneNode *node);

  // Recursively imports a tree of nodes from FCollada, rooted at the
  // given node, into the O3D scene.
  void ImportTree(NodeInstance *instance,
                  Transform* parent,
                  ParamFloat* animation_input);

  // Recursively imports a tree of instances (shapes, etc..) from FCollada,
  // rooted at the given node, into the O3D scene. This is a separate step
  // from ImportTree because various kinds of instances can reference other
  // parts of the tree.
  void ImportTreeInstances(FCDocument* doc,
                           NodeInstance* instance);

  bool BuildFloatAnimation(ParamFloat* result,
                           FCDAnimated* animated,
                           const char* qualifier,
                           ParamFloat* animation_input,
                           float output_scale,
                           float default_value);

  bool BuildFloat3Animation(ParamFloat3* result, FCDAnimated* animated,
                            ParamFloat* animation_input,
                            const Float3& default_value);

  ParamMatrix4* BuildComposition(FCDTMatrix* transform,
                                 ParamMatrix4* input_matrix,
                                 ParamFloat* animation_input);

  ParamMatrix4* BuildComposition(const Matrix4& matrix,
                                 ParamMatrix4* input_matrix);

  ParamMatrix4* BuildTranslation(FCDTTranslation* transform,
                                 ParamMatrix4* input_matrix,
                                 ParamFloat* animation_input);

  ParamMatrix4* BuildRotation(FCDTRotation* transform,
                              ParamMatrix4* input_matrix,
                              ParamFloat* animation_input);

  ParamMatrix4* BuildScaling(FCDTScale* transform,
                             ParamMatrix4* input_matrix,
                             ParamFloat* animation_input);

  // Builds a Transform node corresponding to the transform elements of
  // a given node.  All transformations (rotation, translation, scale,
  // etc) are collapsed into a single Transform.
  Transform* BuildTransform(FCDSceneNode* node,
                            Transform* parent_transform,
                            ParamFloat* animation_input);

  // Extracts the various camera parameters from a Collada Camera object and
  // stored them as Params on an O3D Transform.
  void BuildCamera(FCDocument* doc,
                   FCDCamera* camera,
                   Transform* transform,
                   FCDSceneNode* parent_node);

  // Gets an O3D shape corresponding to a given FCollada geometry instance.
  // If the Shape does not exist, Builds one.
  Shape* GetShape(FCDocument* doc,
                  FCDGeometryInstance* geom_instance);

  // Builds O3D shape corresponding to a given FCollada geometry instance.
  Shape* BuildShape(FCDocument* doc,
                    FCDGeometryInstance* geom_instance,
                    FCDGeometry* geom,
                    TranslationMap* translationMap);

  // Gets an O3D skinned shape corresponding to a given FCollada controller
  // instance. If the Shape does not exist, Builds one.
  Shape* GetSkinnedShape(FCDocument* doc,
                         FCDControllerInstance* instance,
                         NodeInstance *parent_node_instance);

  // Builds O3D skinned shape corresponding to a given FCollada controller
  // instance.
  Shape* BuildSkinnedShape(FCDocument* doc,
                           FCDControllerInstance* instance,
                           NodeInstance *parent_node_instance);

  // Builds an O3D texture corresponding to a given FCollada surface
  // parameter.
  Texture* BuildTexture(FCDEffectParameterSurface* surface);

  // Builds an O3D texture corresponding to a given FCDImage.
  Texture* BuildTextureFromImage(FCDImage* image);

  // Builds an O3D material from a COLLADA effect and material.  If a
  // COLLADA-FX (Cg/HLSL) effect is present, it will be used and a programmable
  // Effect generated.  If not, an attempt is made to use one of the fixed-
  // function profiles if present (eg., Constant, Lambert).
  Material* BuildMaterial(FCDocument* doc, FCDMaterial* material);

  // Gets an O3D effect correpsonding to a given FCollada effect. If the
  // effect does not already exist it is created.
  Effect* GetEffect(FCDocument* doc, FCDEffect* effect);

  // Builds an O3D effect from a COLLADA effect and material.  If a
  // COLLADA-FX (Cg/HLSL) effect is present, it will be used and a programmable
  // Effect generated.  If not, an attempt is made to use one of the fixed-
  // function profiles if present (eg., Constant, Lambert).
  Effect* BuildEffect(FCDocument* doc, FCDEffect* effect);

  // Copies the texture sampler states from an FCollada sampler to an O3D
  // sampler.
  void SetSamplerStates(FCDEffectParameterSampler* effect_sampler,
                        Sampler* o3d_sampler);

  // Converts the given COLLADA pass state into one or more state
  // parameters on the given O3D state object.
  void AddRenderState(FCDEffectPassState* pass_state, State* state);

  // Sets the O3D culling state based on the OpenGL-style states that
  // COLLADA-FX uses, cached in cull_enabled_, cull_front_ and front_cw_.
  void UpdateCullingState(State* state);

  // Sets an O3D parameter value from a given FCollada effect parameter.
  bool SetParamFromFCEffectParam(ParamObject *param_object,
                                 const String &param_name,
                                 FCDEffectParameter *fc_param);

  // Sets the value of a Param on the given ParamObject from an FCollada
  // standard-profile effect parameter.  If the FCollada parameter
  // contains a texture, the sampler_param_name and channel is used to set
  // a Sampler Param in o3d from the surface.  If not, the
  // color_param_name is used to create set a vector Param value.
  void SetParamFromStandardEffectParam(FCDEffectStandard* effect_standard,
                                       ParamObject* param_object,
                                       const char* color_param_name,
                                       const char* sampler_param_name,
                                       FCDEffectParameter* color_param,
                                       int channel);

  // Sets the values of shape parameters from a given FCollada material node.
  // If a corresponding shape parameter is not found, the FCollada parameter
  // is ignored.
  void SetParamsFromMaterial(FCDMaterial* material, ParamObject* param_object);

  // Finds a node instance corresponding to a scene node. Since a particular
  // scene node can be instanced multiple times, this will return an arbitrary
  // instance.
  NodeInstance *FindNodeInstance(FCDSceneNode *node);

  // Finds the node instance corresponding to a scene node if it is not
  // instanced, by following the only parent of the nodes until it reaches the
  // root. This will return NULL if the node can't be found, or if the node is
  // instanced more than once.
  NodeInstance *FindNodeInstanceFast(FCDSceneNode *node);

  // Clears out any residual data from the last import.  Doesn't
  // affect the Pack or the options, just intermediate data structures
  // in this object.
  void ClearData();

  // Gets a dummy effect or creates one if none exists.
  Effect* GetDummyEffect();

  // Gets a dummy material or creates one if none exists.
  Material* GetDummyMaterial();

  ServiceLocator* service_locator_;

  // The object from which error status is retreived.
  IErrorStatus* error_status_;

  // The Pack into which newly-created nodes will be placed.
  Pack* pack_;

  // The import options to use;
  Options options_;

  // The effect used if we can't create an effect.
  Effect* dummy_effect_;

  // The material used if we can't create a material.
  Material* dummy_material_;

  // The root of the instance node tree.
  NodeInstance* instance_root_;

  // A map of the Textures created by the importer, indexed by filename.
  std::map<const std::wstring, Texture*> textures_;

  // A map containing the original data (still in original format)
  // used to create the textures, sounds, etc., indexed by filename.
  typedef std::map<FilePath, std::string> OriginalDataMap;
  OriginalDataMap original_data_;

  // A map of the Effects created by the importer, indexed by DAE id.
  std::map<const std::string, Effect*> effects_;

  // A map of the Shapes created by the importer, indexed by DAE id.
  std::map<const std::string, Shape*> shapes_;

  // A map of the Skinned Shapes created by the importer, indexed by DAE id.
  std::map<const std::string, Shape*> skinned_shapes_;

  // A map of the Materials created by the importer, indexed by DAE id.
  std::map<const std::string, Material*> materials_;

  // All the errors accumlated during loading.
  String errors_;

  // The absolute path to the top of the model hierarchy, to use for
  // determining the relative paths to other files.
  FilePath base_path_;

  ColladaZipArchive *collada_zip_archive_;
  // Some temporaries used by the state importer
  bool cull_enabled_;
  bool cull_front_;
  bool front_cw_;

  int unique_filename_counter_;

  DISALLOW_COPY_AND_ASSIGN(Collada);
};
}
#endif  // O3D_IMPORT_CROSS_COLLADA_H_
