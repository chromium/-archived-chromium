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


// Pack file.

#include "base/file_path.h"
#include "core/cross/precompile.h"
#include "core/cross/pack.h"
#include "core/cross/bitmap.h"
#include "core/cross/draw_context.h"
#include "import/cross/archive_request.h"
#include "import/cross/raw_data.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "core/cross/file_request.h"
#include "core/cross/render_node.h"
#include "core/cross/iclass_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "utils/cross/file_path_utils.h"

namespace o3d {

O3D_DEFN_CLASS(Pack, NamedObject)

Pack::Pack(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      class_manager_(service_locator->GetService<IClassManager>()),
      object_manager_(service_locator->GetService<ObjectManager>()),
      renderer_(service_locator->GetService<Renderer>()) {
}

Pack::~Pack() {
}

bool Pack::Destroy() {
  return object_manager_->DestroyPack(this);
}

bool Pack::RemoveObject(ObjectBase* object) {
  return UnregisterObject(object);
}

Transform* Pack::root() const {
  return root_.Get();
}

void Pack::set_root(Transform *root) {
  root_ = Transform::Ref(root);
}

ObjectBase* Pack::CreateObject(const String& type_name) {
  ObjectBase::Ref new_object(class_manager_->CreateObject(type_name));
  if (new_object.Get() != NULL) {
    RegisterObject(new_object);
    return new_object.Get();
  }
  return NULL;
}

ObjectBase* Pack::CreateObjectByClass(const ObjectBase::Class* type) {
  ObjectBase::Ref new_object(class_manager_->CreateObjectByClass(type));
  if (new_object.Get() != NULL) {
    RegisterObject(new_object);
    return new_object.Get();
  }
  return NULL;
}

// Creates a new FileRequest object.
FileRequest *Pack::CreateFileRequest(const String& type) {
  FileRequest *request = FileRequest::Create(service_locator(),
                                             this,
                                             FileRequest::TypeFromString(type));
  if (request) {
    RegisterObject(request);
  }
  return request;
}

// Creates a new ArchiveRequest object.
ArchiveRequest *Pack::CreateArchiveRequest() {
  ArchiveRequest *request = ArchiveRequest::Create(service_locator(),
                                                   this);
  if (request) {
    RegisterObject(request);
  }
  return request;
}

// Creates a Texture object from a file in the current render context format.
Texture* Pack::CreateTextureFromFile(const String& uri,
                                     const FilePath& filepath,
                                     Bitmap::ImageFileType file_type,
                                     bool generate_mipmaps) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  String filename = FilePathToUTF8(filepath);

  DLOG(INFO) << "CreateTextureFromFile(uri='" << uri
             << "', filename='" << filename << "')";

  // TODO: Add support for volume texture when we have code to load
  //                  them
  Bitmap bitmap;
  if (!bitmap.LoadFromFile(filepath, file_type, generate_mipmaps)) {
    O3D_ERROR(service_locator())
        << "Failed to load bitmap file \"" << uri << "\"";
    return NULL;
  }

  return CreateTextureFromBitmap(&bitmap, uri);
}

// Creates a Texture object from a file in the current render context format.
// This version takes a String |filename| argument instead of the preferred
// FilePath argument.  The use of this method should be phased out
Texture* Pack::CreateTextureFromFile(const String& uri,
                                     const String& filename,
                                     Bitmap::ImageFileType file_type,
                                     bool generate_mipmaps) {
  FilePath filepath = UTF8ToFilePath(filename);
  return CreateTextureFromFile(uri,
                               filepath,
                               file_type,
                               generate_mipmaps);
}

// Creates a Texture object from a bitmap in the current render context format.
Texture* Pack::CreateTextureFromBitmap(Bitmap *bitmap, const String& uri) {
  DCHECK(bitmap);

  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (bitmap->width() > Texture::MAX_DIMENSION ||
      bitmap->height() > Texture::MAX_DIMENSION) {
    O3D_ERROR(service_locator())
        << "Texture (uri='" << uri
        << "', size="  << bitmap->width() << "x" << bitmap->height()
        << ", mips=" << bitmap->num_mipmaps()<< ") is larger than the "
        << "maximum texture size which is (" << Texture::MAX_DIMENSION
        << "x" << Texture::MAX_DIMENSION << ")";
    return NULL;
  }

  Texture::Ref texture = renderer_->CreateTextureFromBitmap(bitmap);

  if (!texture.IsNull()) {
    ParamString* param = texture->CreateParam<ParamString>(
        O3D_STRING_CONSTANT("uri"));
    DCHECK(param != NULL);
    param->set_value(uri);

    RegisterObject(texture);
  } else {
    O3D_ERROR(service_locator())
        << "Unable to create texture (uri='" << uri
        << "', size="  << bitmap->width() << "x" << bitmap->height()
        << ", mips=" << bitmap->num_mipmaps()<< ")";
  }

  return texture.Get();
}


// Creates a Texture object from RawData and allocates
// the necessary resources for it.
Texture* Pack::CreateTextureFromRawData(RawData *raw_data,
                                        bool generate_mips) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  const String uri = raw_data->uri();

  DLOG(INFO) << "CreateTextureFromRawData(uri='" << uri << "')";


  Bitmap bitmap;
  if (!bitmap.LoadFromRawData(raw_data, Bitmap::UNKNOWN, generate_mips)) {
    O3D_ERROR(service_locator())
        << "Failed to load bitmap from raw data \"" << uri << "\"";
    return NULL;
  }

  return CreateTextureFromBitmap(&bitmap, uri);
}

// Creates a Texture2D object and allocates the necessary resources for it.
Texture2D* Pack::CreateTexture2D(int width,
                                 int height,
                                 Texture::Format format,
                                 int levels,
                                 bool enable_render_surfaces) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (width > Texture::MAX_DIMENSION ||
      height > Texture::MAX_DIMENSION) {
    O3D_ERROR(service_locator())
        << "Maximum texture size is (" << Texture::MAX_DIMENSION << "x"
        << Texture::MAX_DIMENSION << ")";
    return NULL;
  }

  if (enable_render_surfaces) {
    if (Bitmap::GetPOTSize(width) != width ||
        Bitmap::GetPOTSize(height) != height) {
      O3D_ERROR(service_locator()) <<
          "Textures with RenderSurfaces enabled must have power-of-two "
          "dimensions.";
      return NULL;
    }
  }

  Texture2D::Ref texture = renderer_->CreateTexture2D(
      width,
      height,
      format,
      (levels == 0) ? Bitmap::GetMipMapCount(width,
                                             height) : levels,
      enable_render_surfaces);
  if (!texture.IsNull()) {
    RegisterObject(texture);
  }

  return texture.Get();
}

// Creates a TextureCUBE object and allocates the necessary resources for it.
TextureCUBE* Pack::CreateTextureCUBE(int edge_length,
                                     Texture::Format format,
                                     int levels,
                                     bool enable_render_surfaces) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (edge_length > Texture::MAX_DIMENSION) {
    O3D_ERROR(service_locator())
        << "Maximum edge_length is " << Texture::MAX_DIMENSION;
    return NULL;
  }


  if (enable_render_surfaces) {
    if (Bitmap::GetPOTSize(edge_length) != edge_length) {
      O3D_ERROR(service_locator()) <<
          "Textures with RenderSurfaces enabled must have power-of-two "
          "dimensions.";
      return NULL;
    }
  }

  TextureCUBE::Ref texture = renderer_->CreateTextureCUBE(
      edge_length,
      format,
      (levels == 0) ? Bitmap::GetMipMapCount(edge_length,
                                             edge_length) : levels,
      enable_render_surfaces);
  if (!texture.IsNull()) {
    RegisterObject(texture);
  }

  return texture.Get();
}

RenderDepthStencilSurface* Pack::CreateDepthStencilSurface(int width,
                                                           int height) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (width > Texture::MAX_DIMENSION ||
      height > Texture::MAX_DIMENSION) {
    O3D_ERROR(service_locator())
        << "Maximum texture size is (" << Texture::MAX_DIMENSION << "x"
        << Texture::MAX_DIMENSION << ")";
    return NULL;
  }

  if (Bitmap::GetPOTSize(width) != width ||
      Bitmap::GetPOTSize(height) != height) {
    O3D_ERROR(service_locator()) <<
        "Depth-stencil RenderSurfaces must have power-of-two dimensions.";
    return NULL;
  }

  RenderDepthStencilSurface::Ref surface =
      renderer_->CreateDepthStencilSurface(width, height);

  if (!surface.IsNull()) {
    RegisterObject(surface);
  }
  return surface.Get();
}

ObjectBase* Pack::GetObjectBaseById(Id id,
                                    const ObjectBase::Class* class_type) {
  ObjectBase::Ref object(object_manager_->GetObjectBaseById(id, class_type));
  if (!object.IsNull() &&
      owned_objects_.find(object) == owned_objects_.end()) {
    object.Reset();
  }
  return object.Get();
}

ObjectBaseArray Pack::GetObjects(const String& name,
                                 const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectSet::const_iterator end(owned_objects_.end());
  for (ObjectSet::const_iterator iter(owned_objects_.begin());
       iter != end;
       ++iter) {
    ObjectBase* object = iter->Get();
    if (object->IsAClassName(class_type_name)) {
      if (object->IsA(NamedObjectBase::GetApparentClass())) {
        if (name.compare(down_cast<NamedObjectBase*>(object)->name()) == 0) {
          objects.push_back(object);
        }
      }
    }
  }
  return objects;
}

ObjectBaseArray Pack::GetObjectsByClassName(
    const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectSet::const_iterator end(owned_objects_.end());
  for (ObjectSet::const_iterator iter(owned_objects_.begin());
       iter != end;
       ++iter) {
    if (iter->Get()->IsAClassName(class_type_name)) {
      objects.push_back(iter->Get());
    }
  }
  return objects;
}

void Pack::RegisterObject(ObjectBase *object) {
  ObjectBase::Ref temp(object);
  DLOG_ASSERT(owned_objects_.find(temp) == owned_objects_.end())
      << "attempt to register duplicate object in pack.";
  owned_objects_.insert(temp);
}

bool Pack::UnregisterObject(ObjectBase *object) {
  ObjectSet::iterator find(owned_objects_.find(ObjectBase::Ref(object)));
  if (find == owned_objects_.end())
    return false;

  owned_objects_.erase(find);
  return true;
}
}  // namespace o3d
