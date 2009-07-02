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


// This file contains the definition for resource classes and the resource map.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_RESOURCE_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_RESOURCE_H__

#include <vector>
#include "base/scoped_ptr.h"
#include "core/cross/types.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

// Base class for resources, just providing a common Destroy function.
class Resource {
 public:
  Resource() {}
  virtual ~Resource() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// VertexBuffer class, representing a vertex buffer resource.
class VertexBuffer: public Resource {
 public:
  VertexBuffer(unsigned int size, unsigned int flags)
      : size_(size),
        flags_(flags) {}
  virtual ~VertexBuffer() {}

  // Returns the vertex buffer flags.
  unsigned int flags() const { return flags_; }
  // Sets the vertex buffer flags.
  void set_flags(unsigned int flags) { flags_ = flags; }
  // Returns the vertex buffer size.
  unsigned int size() const { return size_; }
  // Sets the vertex buffer size.
  void set_size(unsigned int size) { size_ = size; }
 protected:
  unsigned int size_;
  unsigned int flags_;
 private:
  DISALLOW_COPY_AND_ASSIGN(VertexBuffer);
};

// IndexBuffer class, representing an index buffer resource.
class IndexBuffer: public Resource {
 public:
  IndexBuffer(unsigned int size, unsigned int flags)
      : size_(size),
        flags_(flags) {}
  virtual ~IndexBuffer() {}

  // Returns the index buffer flags.
  unsigned int flags() const { return flags_; }
  // Sets the index buffer flags.
  void set_flags(unsigned int flags) { flags_ = flags; }
  // Returns the index buffer size.
  unsigned int size() const { return size_; }
  // Sets the index buffer size.
  void set_size(unsigned int size) { size_ = size; }
 protected:
  unsigned int size_;
  unsigned int flags_;
 private:
  DISALLOW_COPY_AND_ASSIGN(IndexBuffer);
};

// VertexStruct class, representing a vertex struct resource.
class VertexStruct: public Resource {
 public:
  // The representation of an input data stream.
  struct Element {
    ResourceID vertex_buffer;
    unsigned int offset;
    unsigned int stride;
    vertex_struct::Type type;
    vertex_struct::Semantic semantic;
    unsigned int semantic_index;
  };

  explicit VertexStruct(unsigned int count)
      : count_(count),
        elements_(new Element[count]) {
    memset(elements_.get(), 0, count * sizeof(Element));  // NOLINT
  }

  // Returns the number of inputs in this struct.
  unsigned int count() const { return count_; }
  // Returns an element by index.
  Element &GetElement(unsigned int i) {
    DCHECK_GT(count_, i);
    return elements_[i];
  }
 protected:
  unsigned int count_;
  scoped_array<Element> elements_;
 private:
  DISALLOW_COPY_AND_ASSIGN(VertexStruct);
};

// Effect class, representing an effect.
class Effect: public Resource {
 public:
  Effect() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(Effect);
};

// EffectParam class, representing an effect parameter.
class EffectParam: public Resource {
 public:
  explicit EffectParam(effect_param::DataType data_type)
      : data_type_(data_type) {
  }

  // Gets the data type of this parameter.
  effect_param::DataType data_type() const { return data_type_; }
 private:
  effect_param::DataType data_type_;
  DISALLOW_COPY_AND_ASSIGN(EffectParam);
};

class EffectStream: public Resource {
 public:
  explicit EffectStream() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(EffectStream);
};

// Texture class, representing a texture resource.
class Texture: public Resource {
 public:
  Texture(texture::Type type,
          unsigned int levels,
          texture::Format format,
          unsigned int flags)
      : type_(type),
        levels_(levels),
        format_(format),
        flags_(flags) {}
  virtual ~Texture() {}

  // Returns the type of the texture.
  texture::Type type() const { return type_; }
  // Returns the texture flags.
  unsigned int flags() const { return flags_; }
  // Returns the texture format.
  texture::Format format() const { return format_; }
  // Returns the number of mipmap levels in the texture.
  unsigned int levels() const { return levels_; }
 private:
  texture::Type type_;
  unsigned int levels_;
  texture::Format format_;
  unsigned int flags_;
  DISALLOW_COPY_AND_ASSIGN(Texture);
};

// Texture class, representing a sampler resource.
class Sampler: public Resource {
 public:
  Sampler() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(Sampler);
};

// Base of ResourceMap. Contains most of the implementation of ResourceMap, to
// avoid template bloat.
class ResourceMapBase {
 public:
  ResourceMapBase() : resources_() {}
  ~ResourceMapBase() {}

  // Assigns a resource to a resource ID. Assigning a resource to an ID that
  // already has an existing resource will destroy that existing resource. The
  // map takes ownership of the resource.
  void Assign(ResourceID id, Resource* resource);
  // Destroys a resource.
  bool Destroy(ResourceID id);
  // Destroy all resources.
  void DestroyAllResources();
  // Gets a resource by ID.
  Resource *Get(ResourceID id) {
    return (id < resources_.size()) ? resources_[id] : NULL;
  }
 private:
  typedef std::vector<Resource *> Container;
  Container resources_;
};

// Resource Map class, allowing resource ID <-> Resource association. This is a
// dense map, optimized for retrieval (O(1)).
template<class T> class ResourceMap {
 public:
  ResourceMap() : container_() {}
  ~ResourceMap() {}

  // Assigns a resource to a resource ID. Assigning a resource to an ID that
  // already has an existing resource will destroy that existing resource. The
  // map takes ownership of the resource.
  void Assign(ResourceID id, T* resource) {
    container_.Assign(id, resource);
  }
  // Destroys a resource.
  bool Destroy(ResourceID id) {
    return container_.Destroy(id);
  }
  // Destroy all resources.
  void DestroyAllResources() {
    return container_.DestroyAllResources();
  }
  // Gets a resource by ID.
  T *Get(ResourceID id) {
    return down_cast<T*>(container_.Get(id));
  }
 private:
  ResourceMapBase container_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_RESOURCE_H__
