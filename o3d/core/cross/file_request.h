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


// This file contains the declaration of the FileRequest class.

#ifndef O3D_CORE_CROSS_FILE_REQUEST_H_
#define O3D_CORE_CROSS_FILE_REQUEST_H_

#include <algorithm>
#include <string>

#include "base/scoped_ptr.h"
#include "core/cross/callback.h"
#include "core/cross/object_base.h"
#include "core/cross/pack.h"

namespace o3d {

typedef Closure FileRequestCallback;

// A FileRequest object is used to carry out an asynchronous request for a file
// to be loaded.
class FileRequest : public ObjectBase {
 public:
  enum Type {
    TYPE_INVALID,
    TYPE_TEXTURE,
    TYPE_MAX = TYPE_TEXTURE,
  };

  enum ReadyState {  // These are copied from XMLHttpRequest.
    STATE_INIT = 0,
    STATE_OPEN = 1,
    STATE_SENT = 2,
    STATE_RECEIVING = 3,
    STATE_LOADED = 4,
  };

 public:
  typedef SmartPointer<FileRequest> Ref;

  virtual ~FileRequest() { }

  static FileRequest *Create(ServiceLocator* service_locator,
                             Pack *pack,
                             Type type);

  static Type TypeFromString(String type) {
    std::transform(type.begin(),
                   type.end(),
                   type.begin(),
        ::tolower);
    if (type == "texture") {
      return TYPE_TEXTURE;
    }
    return TYPE_INVALID;
  }

  static bool IsValidType(Type type) {
    return type > TYPE_INVALID && type <= TYPE_MAX;
  }
  Pack *pack() const {
    return pack_.Get();  // Set at creation time and never changed.
  }
  FileRequestCallback *onreadystatechange() const {
    return onreadystatechange_.get();
  }
  void set_onreadystatechange(FileRequestCallback *onreadystatechange) {
    onreadystatechange_.reset(onreadystatechange);
  }
  const String& uri() const {
    return uri_;
  }
  void set_uri(const String& uri) {
    uri_ = uri;
  }
  Type type() const {
    return type_;  // Set at creation time and never changed.
  }
  Texture *texture() const {
    return texture_.Get();
  }
  bool generate_mipmaps() const { return generate_mipmaps_; }
  void set_generate_mipmaps(bool value) {
    generate_mipmaps_ = value;
  }
  void set_texture(Texture *texture) {
    CHECK(type_ == TYPE_TEXTURE);
    texture_ = Texture::Ref(texture);
  }
  bool done() const {
    return done_;
  }
  bool success() const {
    return success_;
  }
  void set_success(bool success) {
    success_ = success;
    done_ = true;
    pack_.Reset();  // Removes pack reference, allowing pack garbage collection.
  }
  int ready_state() const {
    return ready_state_;
  }
  void set_ready_state(int state) {
    ready_state_ = state;
  }
  const String& error() const {
    return error_;
  }
  void set_error(const String& error) {
    error_ = error;
  }

  O3D_DECL_CLASS(FileRequest, ObjectBase);

 private:
  FileRequest(ServiceLocator* service_locator,
              Pack *pack,
              Type type);

  Pack::Ref pack_;
  scoped_ptr<FileRequestCallback> onreadystatechange_;
  String uri_;
  Type type_;
  Texture::Ref texture_;  // Only used on a successful texture load.
  bool generate_mipmaps_;
  bool done_;  // Set after completion/failure to indicate success_ is valid.
  bool success_;  // Set after completion/failure to indicate which it is.
  int ready_state_;  // Like the XMLHttpRequest variable of the same name.
  String error_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileRequest);
};  // FileRequest

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FILE_REQUEST_H_
