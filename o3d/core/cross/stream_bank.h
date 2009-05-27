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


// This file cotnains the declaration of StreamBank.

#ifndef O3D_CORE_CROSS_STREAM_BANK_H_
#define O3D_CORE_CROSS_STREAM_BANK_H_

#include <vector>
#include <map>
#include "core/cross/element.h"
#include "core/cross/stream.h"

namespace o3d {

class VertexSource;

// A StreamBank collects streams so they can be shared amoung StreamBanks. It
// also handles platform specific things like vertex declarations..
class StreamBank : public NamedObject {
 public:
  typedef SmartPointer<StreamBank> Ref;
  typedef WeakPointer<StreamBank> WeakPointerType;

  virtual ~StreamBank();

  // The number of times streams have been added or removed from this stream
  // bank. Can be used for caching.
  int change_count() {
    return change_count_;
  }

  // Binds a field of a vertex buffer to the streambank and defines how the data
  // in the buffer should be accessed and interpreted. The buffer of the field
  // must be of a compatible type otherwise the binding fails and the function
  // returns false.
  virtual bool SetVertexStream(Stream::Semantic semantic,
                               int semantic_index,
                               Field* field,
                               unsigned int start_index);

  // Searches the vertex streams bound to the shape for one with the given
  // stream semantic.  If a stream is not found then it returns NULL.
  const Stream* GetVertexStream(Stream::Semantic stream_semantic,
                                int semantic_index) const;

  // Removes a vertex stream from this primitive.
  // Returns true if the specified stream existed.
  bool RemoveVertexStream(Stream::Semantic stream_semantic,
                          int semantic_index);

  // Returns the maximum vertices available given the streams currently
  // set on this StreamBank.
  unsigned GetMaxVertices() const;

  // Used by BindStream.
  // Returns the ParamVertexBufferStream that manages the given stream.
  // as an output param for this VertexSource.
  ParamVertexBufferStream* GetVertexStreamParam(Stream::Semantic semantic,
                                                int semantic_index) const;

  const StreamParamVector& vertex_stream_params() const {
    return vertex_stream_params_;
  }

  // Bind the source stream to the corresponding stream in this VertexSource.
  // Parameters:
  //   source: Source to get vertices from.
  //   semantic: The semantic of the vertices to get
  //   semantic_index: The semantic index of the vertices to get.
  // Returns:
  //   True if success. False if failure. If the requested semantic or semantic
  //   index do not exist on the source or this source the bind will fail. If
  //   they do exist but are not the same length the bind will fail.
  bool BindStream(VertexSource* source,
                  Stream::Semantic semantic,
                  int semantic_index);

  // Unbinds the requested stream.
  // Parameters:
  //   semantic: The semantic of the vertices to unbind
  //   semantic_index: The semantic index of the vertices to unbind.
  // Returns:
  //   True if unbound. False those vertices do not exist or were not bound.
  bool UnbindStream(Stream::Semantic semantic, int semantic_index);

  // If the streams are bound to other streams, update them.
  void UpdateStreams();

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 protected:
  explicit StreamBank(ServiceLocator* service_locator);

  class SlaveParamVertexBufferStream : public ParamVertexBufferStream {
   public:
    typedef SmartPointer<SlaveParamVertexBufferStream> Ref;
    SlaveParamVertexBufferStream(ServiceLocator* service_locator,
                                 StreamBank* master,
                                 Stream* stream)
        : ParamVertexBufferStream(service_locator, stream, true, false),
          master_(master) {
    }

    virtual void CopyDataFromParam(Param* source_param) {
      // do nothing.
    }

    virtual void OnAfterBindInput() {
      ++master_->number_binds_;
    }

    virtual void OnAfterUnbindInput(Param* old_source) {
      master_->number_binds_;
    }

   private:
    StreamBank* master_;
    DISALLOW_COPY_AND_ASSIGN(SlaveParamVertexBufferStream);
  };

  // Called after the a stream as been added or removed.
  // Overridden in derived classes that need to know when a stream has been
  // added or removed.
  virtual void OnUpdateStreams() { }

  StreamParamVector vertex_stream_params_;

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // the number of streams that are bound to a VertexSource.
  // Used as a shortcut. If zero no need to do expensive checking.
  unsigned int number_binds_;

  // The number of times a stream has been added or removed.
  int change_count_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(StreamBank, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(StreamBank);
};

class ParamStreamBank : public TypedRefParam<StreamBank> {
 public:
  typedef SmartPointer<ParamStreamBank> Ref;

  ParamStreamBank(ServiceLocator* service_locator,
                  bool dynamic,
                  bool read_only)
      : TypedRefParam<StreamBank>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamStreamBank, RefParamBase)
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_STREAM_BANK_H_
