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

// This file implements the asynchronous file-loading glue.

#include <algorithm>

#include "plugin/cross/async_loading.h"
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/stream_manager.h"
#include "core/cross/bitmap.h"
#include "core/cross/error_status.h"
#include "core/cross/file_request.h"
#include "core/cross/pack.h"
#include "core/cross/texture.h"

namespace glue {
namespace namespace_o3d {
namespace class_FileRequest {

using _o3d::PluginObject;
using o3d::Bitmap;
using o3d::Pack;
using o3d::Texture;

// StreamManager::FinishedCallback
// implementation that imports the file as a texture once downloaded.
// When the download completes, LoadTextureURLCallback::Run() will be called,
// which will parse and load the downloaded file.  After that load is complete,
// onreadystatechange will be run to notify the user.
class LoadTextureURLCallback : public StreamManager::FinishedCallback {
 public:
  // Creates a new LoadTextureURLCallback.
  static LoadTextureURLCallback *Create(FileRequest *request) {
    return new LoadTextureURLCallback(request);
  }

  virtual ~LoadTextureURLCallback() {
    // If the file request was interrupted (for example we moved to a new page
    // before the file transfer was completed) then we tell the FileRequest
    // object that the request failed.  It's important to call this here since
    // set_success() will release the pack reference that the FileRequest holds
    // which will allow the pack to be garbage collected.
    if (!request_->done()) {
      request_->set_success(false);
    }
  }

  // Loads the texture file, calls the JS callback to pass back the texture
  // object.
  virtual void Run(DownloadStream*,
                   bool success,
                   const std::string &filename,
                   const std::string &mime_type) {
    Texture::Ref texture;
    if (success) {
      o3d::ErrorCollector error_collector(request_->service_locator());
      request_->set_ready_state(FileRequest::STATE_LOADED);
      // Try to get the image file type from the returned MIME type.
      // Unfortunately, TGA and DDS don't have standard MIME type, so we may
      // have to rely on the filename, or let the image loader figure it out by
      // itself (by trying every possible type).
      Bitmap::ImageFileType image_type =
          Bitmap::GetFileTypeFromMimeType(mime_type.c_str());
      texture = Texture::Ref(
          request_->pack()->CreateTextureFromFile(
              request_->uri(),
              filename.c_str(),
              image_type,
              request_->generate_mipmaps()));
      if (texture) {
        texture->set_name(request_->uri());
        request_->set_texture(texture);
      } else {
        success = false;
      }
      request_->set_error(error_collector.errors());
    } else {
      // No error is passed in from the stream but we MUST have an error
      // for the request to work on the javascript side.
      request_->set_error("Could not download texture: " + request_->uri());
    }
    request_->set_success(success);
    // Since the standard codes only go far enough to tell us that the download
    // succeeded, we set the success [and implicitly the done] flags to give the
    // rest of the story.
    if (request_->onreadystatechange())
      request_->onreadystatechange()->Run();
  }

 private:
  FileRequest::Ref request_;

  explicit LoadTextureURLCallback(FileRequest *request)
      : request_(request) {
  }
};

// Sets up the parameters required for all FileRequests.
void userglue_method_open(void *plugin_data,
                          FileRequest *request,
                          const String &method,
                          const String &uri,
                          bool async) {
  if (!async) {
    request->set_success(false);
    O3D_ERROR(request->service_locator())
        << ("synchronous request not supported");
    return;  // We don't yet support synchronous requests.
  }
  if (request->done()) {
    request->set_success(false);
    request->set_ready_state(FileRequest::STATE_INIT);  // Show we're unready.
    O3D_ERROR(request->service_locator())
        << "request can not be reused";
    return;  // We don't yet support reusing FileRequests.
  }

  String method_lower(method);
  std::transform(method.begin(), method.end(), method_lower.begin(), ::tolower);
  if (method_lower != "get") {
    request->set_success(false);
    O3D_ERROR(request->service_locator())
        << "request does not support POST yet";
    return;  // We don't yet support fetching files via POST.
  }
  request->set_uri(uri);
  request->set_ready_state(FileRequest::STATE_OPEN);
}

// Starts downloading or reading the requested file, passing in a callback that
// will parse and incorporate the file upon success.
void userglue_method_send(void *plugin_data,
                          FileRequest *request) {
  PluginObject *plugin_object = static_cast<PluginObject *>(plugin_data);
  StreamManager *stream_manager = plugin_object->stream_manager();
  StreamManager::FinishedCallback *callback = NULL;
  bool result = false;

  if (request->done()) {
    request->set_success(false);
    O3D_ERROR(request->service_locator())
        << "request can not be reused";
    return;  // FileRequests can't be reused.
  }
  if (request->ready_state() != 1) {  // Forgot to call open, or other error.
    request->set_success(false);
    O3D_ERROR(request->service_locator())
        << "open must be called before send";
    return;
  }
  CHECK(request->pack());

  switch (request->type()) {
  case FileRequest::TYPE_TEXTURE:
    callback = LoadTextureURLCallback::Create(request);
    break;
  default:
    CHECK(false);
  }
  if (callback) {
    DownloadStream *stream =
      stream_manager->LoadURL(request->uri(),
                              NULL,  // new stream callback
                              NULL,  // write ready callback
                              NULL,  // write callback
                              callback,  // finished callback
                              NP_ASFILEONLY);

    if (!stream) {
      request->set_success(false);

      // We don't call O3D_ERROR here because the URI may be user set
      // so we don't want to cause an error callback when the devloper
      // may not be able to know the URI is correct.
      request->set_error("could not create download stream");

      // We need to call the callback to report failure. Because it's async, the
      // code making the request can't know that once it has called send() that
      // the request still exists since send() may have called the callback and
      // the callback may have deleted the request.
      request->onreadystatechange()->Run();
    }

    // If stream is not NULL request may not exist as LoadURL may already have
    // completed and therefore called the callback which may have freed the
    // request so we can't set anything on the request here.
  }
}

}  // namespace class_FileRequest
}  // namespace namespace_o3d
}  // namespace glue
