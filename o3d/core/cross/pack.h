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


// File containing the declaration for the Pack object used to explicitly
// control O3D object lifetimes..

#ifndef O3D_CORE_CROSS_PACK_H_
#define O3D_CORE_CROSS_PACK_H_

#include <vector>
#include <set>

#include "core/cross/bitmap.h"
#include "core/cross/buffer.h"
#include "core/cross/effect.h"
#include "core/cross/named_object.h"
#include "core/cross/sampler.h"
#include "core/cross/shape.h"
#include "core/cross/state.h"
#include "core/cross/smart_ptr.h"
#include "core/cross/texture.h"
#include "core/cross/transform.h"
#include "core/cross/types.h"
#include "core/cross/render_node.h"

namespace o3d {

// Type definitions ------------------------

// Array of object id's
typedef std::vector<Id> IdArray;

class ArchiveRequest;
class RawData;
class FileRequest;
class DrawContext;
class IClassManager;
class ObjectManager;

// A Pack object functions as a container for O3D objects.  All objects
// inheriting from ObjectBase must be constructed and assigned a pack. The Pack
// is used to control the lifetime scope of a collection of objects in bulk. The
// Pack object achieves this by simply storing a set of references to its
// contained objects, which ensures that the ref-counts for those objects never
// reach zero while the pack is alive.
//
// The standard usage model is the following:
//   Pack::Ref pack(client->CreatePack());
//   Transform* transform(pack.Get()->Create<Transform>();
//   ==> Construct multiple nodes and the transform graph.
//   pack.Get()->Destroy();
//   ==> All nodes not referenced by the SceneGraph, or other nodes in live
//   packs are destroyed.
class Pack : public NamedObject {
  friend class ObjectManager;
  friend class ParamObject;
  friend class SmartPointer<Pack>;

  // Forward declaration of private helper class used in std::set typedef below.
  class IdObjectComparator;
 public:
  typedef SmartPointer<Pack> Ref;

  // Set of references to named objects.
  typedef std::set<ObjectBase::Ref, IdObjectComparator> ObjectSet;

  // Removes all internal references to the Pack from the client.
  // The pack, and all objects contained in it are permitted to be destroyed
  // after the packs destruction.  Nodes will only be destoryed after all
  // references to them have been removed.
  // Returns:
  //  True if the pack was successfully deleted.
  bool Destroy();

  // Removes an object from the pack.  The lifetime of the object is no longer
  // bound to the lifetime of the pack. Any object created from a pack.CreateXXX
  // function can be removed.
  // Parameters:
  //   object: Pointer to object to remove.
  // Returns:
  //   true if the object was successfully removed. false if the object is not
  //   part of this pack.
  bool RemoveObject(ObjectBase* object);

  // Creates an Object based on the type_name. This function is for Javascript.
  // Paramaters:
  //  type_name: type name of object type you want created.
  // Returns:
  //  pointer to new object if successful.
  ObjectBase* CreateObject(const String& type_name);

  // Creates an Object based on the type.
  // Parameters:
  //  type: ObjectBase::Class* of the type you want created.
  // Returns:
  //  pointer to new object if successful.
  ObjectBase* CreateObjectByClass(const ObjectBase::Class* type);

  // Creates a Object based on the type. This is a type safe version of
  // CreateObjectByClass for C++.
  // Returns:
  //  pointer to new object if successful.
  template<typename T>
  T* Create() {
    return down_cast<T*>(CreateObjectByClass(T::GetApparentClass()));
  }

  // Creates an Object based on the type_name. This function is for Javascript.
  // Paramaters:
  //  type_name: type name of object type you want created.
  // Returns:
  //  pointer to new object if successful.
  ObjectBase* CreateUnnamedObject(const String& type_name);

  // Creates an Object based on the type.
  // Parameters:
  //  type: ObjectBase::Class* of the type you want created.
  // Returns:
  //  pointer to new object if successful.
  ObjectBase* CreateUnnamedObjectByClass(const ObjectBase::Class* type);

  // Creates a Object based on the type. This is a type safe version of
  // CreateObjectByClass for C++.
  // Returns:
  //  pointer to new object if successful.
  template<typename T>
  T* CreateUnnamed() {
    return down_cast<T*>(CreateUnnamedObjectByClass(T::GetApparentClass()));
  }

  // Transform methods ----

  // Returns the root transform of the pack.  Typically used on import to
  // specify entry into the loaded contents.
  // Returns:
  //  A pointer to the transform assigned as the root.  May be NULL if no root
  //  transform has been assigned.
  Transform* root() const;

  // Assigns the root-transform for the pack. If a root-transform has previously
  // been assigned, it is overwritten with the new value.
  // Parameters:
  //  root: Pointer to transform to assign as the root transform.
  void set_root(Transform* root);

  // Creates a new FileRequest object.  The object is owned by the Client.
  // Parameters:
  //  type: what type of file load will occur after download
  // Returns:
  //  A pointer to the newly created FileRequest, or NULL if creation failed.
  FileRequest* CreateFileRequest(const String& type);

  // Creates a new ArchiveRequest object.  The object is owned by the Client.
  // Returns:
  //  A pointer to the newly created ArchiveRequest, or NULL if creation failed.
  ArchiveRequest* CreateArchiveRequest();

  // Creates a new Texture object from a local file. If the file doesn't exist,
  // or won't load, NULL is returned. The file formats supported are JPEG, PNG,
  // TGA and DDS. If the file contains a cube map, it will be created as an
  // instance of TextureCUBE, otherwise it will be a Texture2D.
  // This function is for internal use only.
  // Parameters:
  //  uri: URI that file was requested from. This is purely for user information
  //      and is not used for loading.
  //  filepath: The local path to the texture file to load
  //  file_type: The file type of the image. If UNKNOWN, it wil be detected
  //             from the extension, or by trying all the possible loaders.
  //  generate_mipmaps: Whether to generate mip-maps or not.
  // Returns:
  //  A pointer to the texture or NULL if it did not load
  Texture* CreateTextureFromFile(const String& uri,
                                 const FilePath& filepath,
                                 Bitmap::ImageFileType file_type,
                                 bool generate_mipmaps);

  // This version takes a String |filename| argument instead of the preferred
  // FilePath argument.  The use of this method should be phased out.
  Texture* CreateTextureFromFile(const String& uri,
                                 const String& filename,
                                 Bitmap::ImageFileType file_type,
                                 bool generate_mipmaps);

  // Creates a new Texture object given a "raw-data" object which must contain
  // binary data in a known image file format (such as JPG or PNG)
  Texture* CreateTextureFromRawData(RawData* raw_data,
                                    bool generate_mips);

  // Creates a new Texture2D object of the specified size and format and
  // reserves the necessary resources for it.
  // Parameters:
  //  width: The width of the texture area in texels
  //  height: The height of the texture area in texels
  //  format: The memory format of each texel
  //  levels: The number of mipmap levels.  Use zero to create the compelete
  //          mipmap chain.
  //  enable_render_surfaces: If true, the texture object will expose
  //                          RenderSurface objects through
  //                          GetRenderSurface(...).
  // Returns:
  //  A pointer to the Texture2D object.
  // Note:  If enable_render_surfaces is true, then the dimensions of the
  // must be a power of two.
  Texture2D* CreateTexture2D(int width,
                             int height,
                             Texture::Format format,
                             int levels,
                             bool enable_render_surfaces);


  // Creates a new TextureCUBE object of the specified size and format and
  // reserves the necessary resources for it.
  // Parameters:
  //  edge_length: The edge of the texture area in texels
  //  format: The memory format of each texel
  //  levels: The number of mipmap levels.  Use zero to create the compelete
  //          mipmap chain.
  //  enable_render_surfaces: If true, the texture object will expose
  //                          RenderSurface objects through
  //                          GetRenderSurface(...).
  // Returns:
  //  A pointer to the TextureCUBE object.
  // Note:  If enable_render_surfaces is true, then the dimensions of the
  // must be a power of two.
  TextureCUBE* CreateTextureCUBE(int edge_length,
                                 Texture::Format format,
                                 int levels,
                                 bool enable_render_surfaces);

  // Creates a new RenderDepthStencilSurface object of D24_S8 format, suitable
  // for use as a depth-stencil render target.
  // Parameters:
  //  width: The width of the surface area in pixels
  //  height: The height of the surface area in pixels
  // Returns:
  //  A pointer to the RenderSurface object.
  // Note: The dimensions of the RenderDepthStencilSurface must be a power of
  //     two.
  RenderDepthStencilSurface* CreateDepthStencilSurface(int width,
                                                       int height);

  // Searches in the Pack for a base object by its id.  If the dynamic type
  // of the object matches the requested type, then a pointer to the instance
  // is returned.
  // Parameters:
  //  id: The id of the ObjectBase to search for.
  //  class_type: The class instance specifier used to filter the return value.
  // Returns:
  //  A pointer to the ObjectBase instance if found and of the matching
  //  dynamic type or NULL otherwise.
  ObjectBase* GetObjectBaseById(Id Id, const ObjectBase::Class* class_type);

  // Searches in the Pack for a base object by its id. Does not check type.
  // This is for Javascript.
  // Parameters:
  //  id: The id of the ObjectBase to search for.
  // Returns:
  //  A pointer to the ObjectBase instance if found NULL otherwise.
  ObjectBase* GetObjectById(Id Id);

  // Searches in the Pack for a base object by its id.  If the dynamic type
  // of the object matches the requested type, then a pointer to the instance
  // is returned.
  // Parameters:
  //  id: The id of the ObjectBase to search for.
  //  class_type: The class instance specifier used to filter the return value.
  // Returns:
  //  A pointer to the ObjectBase instance if found and of the matching
  //  dynamic type or NULL otherwise.
  template<typename T>
  T* GetById(Id id) {
    return down_cast<T*>(GetObjectBaseById(id, T::GetApparentClass()));
  }

  // Search the pack for all objects of a certain class
  // Returns:
  //   Array of Pointers to the requested class.
  template<typename T>
  std::vector<T*> GetByClass() const {
    std::vector<T*> objects;
    ObjectSet::const_iterator end(owned_objects_.end());
    for (ObjectSet::const_iterator iter(owned_objects_.begin());
         iter != end;
         ++iter) {
      if (iter->Get()->IsA(T::GetApparentClass())) {
        objects.push_back(down_cast<T*>(iter->Get()));
      }
    }
    return objects;
  }

  // Get an object by name typesafe. This function is for C++
  // Example:
  //   Buffer* buffer = pack->Get<Buffer>("name");
  // Parameters:
  //   name: name of object to search for.
  // Returns:
  //   std::vector of pointers to type of the objects that matched by name.
  template<typename T>
  std::vector<T*> Get(const String& name) const {
    std::vector<T*> objects;
    if (ObjectBase::ClassIsA(T::GetApparentClass(),
                             NamedObject::GetApparentClass())) {
      ObjectSet::const_iterator end(owned_objects_.end());
      for (ObjectSet::const_iterator iter(owned_objects_.begin());
           iter != end;
           ++iter) {
        if (iter->Get()->IsA(T::GetApparentClass())) {
          if (down_cast<NamedObject*>(
              iter->Get())->name().compare(name) == 0) {
            objects.push_back(down_cast<T*>(iter->Get()));
          }
        }
      }
    }
    return objects;
  }

  // Search the pack for all objects of a certain class with a certain name.
  //
  // This function is for Javascript. Parameters:
  //  name: The name to search for.
  //  class_type_name: the Class of the object. It is okay to pass base types
  //                   for example Node::GetApparentClass()->name will return
  //                   both Transforms and Shapes.
  // Returns:
  //   Array of Object Pointers.
  ObjectBaseArray GetObjects(const String& name,
                             const String& class_type_name) const;

  // Search the pack for all objects of a certain class.
  // This function is for Javascript.
  // Parameters:
  //  class_type_name: the Class of the object. It is okay to pass base types
  //                   for example Node::GetApparentClass()->name will return
  //                   both Transforms and Shapes.
  // Returns:
  //   Array of Object Pointers.
  ObjectBaseArray GetObjectsByClassName(const String& class_type_name) const;

 private:
  // Texture objects function as factories for RenderSurface objects.
  // Constructed RenderSurfaces are registered with the pack associated with
  // the texture, so Texture is befriended to Pack for access to the
  // RegisterObject routine below.
  friend class Texture;

  explicit Pack(ServiceLocator* service_locator);

  virtual ~Pack();

  // Register the given object with the Pack.  The pack will add a reference
  // to the object, guaranteeing its existence as long as the pack has not
  // been destroyed.
  // Parameters:
  //  object: Pointer to a ObjectBase to register within the pack
  void RegisterObject(ObjectBase *object);

  // Unregister a registered object from the pack. If this is the last reference
  // to the object it will be destroyed.
  // Parameters:
  //   object: Pointer to ObjectBase to unregister.
  // Returns:
  //   false if the object was not in the pack.
  bool UnregisterObject(ObjectBase *object);

  // Helper class used as less-than comparator for ordered container classes.
  class IdObjectComparator {
   public:
    // Performs a less than operation on the contents of the left and right
    // smart pointers.
    bool operator()(const ObjectBase::Ref& lhs, const ObjectBase::Ref& rhs)
        const {
      return lhs->id() < rhs->id();
    }
  };

  // Helper method
  Texture* CreateTextureFromBitmap(Bitmap *bitmap, const String& uri);

  IClassManager* class_manager_;
  ObjectManager* object_manager_;
  Renderer* renderer_;

  // The set of objects owned by the pack.  This container contains all of the
  // references that force the lifespan of the contained objects to match
  // or exceed that of the pack.
  ObjectSet owned_objects_;

  Transform::Ref root_;

  O3D_DECL_CLASS(Pack, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(Pack);
};

// Array container for Pack pointers.
typedef std::vector<Pack*> PackArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PACK_H_
