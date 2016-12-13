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


#include "plugin/cross/archive_request_static_glue.h"

#include "base/basictypes.h"
#include "plugin/cross/stream_manager.h"
#include "plugin/cross/o3d_glue.h"
#include "import/cross/archive_request.h"

namespace glue {
namespace namespace_o3d {
namespace class_ArchiveRequest {

using _o3d::PluginObject;
using o3d::Pack;
using o3d::ArchiveRequest;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Callbacks
//
// TODO : get rid of these horrible callback objects which end up just
// dispatching to the ArchiveRequest object.
// Need to change the StreamManager class to implement an interface:
//    WriteReadyCallback
//    WriteCallback
//    FinishedCallback
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveNewStreamCallback : public StreamManager::NewStreamCallback {
 public:
  explicit ArchiveNewStreamCallback(ArchiveRequest *request)
      : request_(request) { }

  virtual void Run(DownloadStream *stream) {
    request_->NewStreamCallback(stream);
  }

 private:
  ArchiveRequest::Ref request_;
  DISALLOW_COPY_AND_ASSIGN(ArchiveNewStreamCallback);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveWriteReadyCallback : public StreamManager::WriteReadyCallback {
 public:
  explicit ArchiveWriteReadyCallback(ArchiveRequest *request)
      : request_(request) { }

  virtual int32 Run(DownloadStream *stream) {
    return request_->WriteReadyCallback(stream);
  }

 private:
  ArchiveRequest::Ref request_;
  DISALLOW_COPY_AND_ASSIGN(ArchiveWriteReadyCallback);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveWriteCallback : public StreamManager::WriteCallback {
 public:
  explicit ArchiveWriteCallback(ArchiveRequest *request) : request_(request) {
  }

  virtual int32 Run(DownloadStream *stream,
                    int32 offset,
                    int32 length,
                    void *data) {
    return request_->WriteCallback(stream, offset, length, data);
  }

 private:
  ArchiveRequest::Ref request_;
  DISALLOW_COPY_AND_ASSIGN(ArchiveWriteCallback);
};

class ArchiveFinishedCallback : public StreamManager::FinishedCallback {
 public:
  explicit ArchiveFinishedCallback(ArchiveRequest *request)
      : request_(request) {
  }

  virtual ~ArchiveFinishedCallback() {
    // If the file request was interrupted (for example we moved to a new page
    // before the file transfer was completed) then we tell the FileRequest
    // object that the request failed.  It's important to call this here since
    // set_success() will release the pack reference that the FileRequest holds
    // which will allow the pack to be garbage collected.
    if (!request_->done()) {
      request_->set_success(false);
    }
  }

  // Loads the Archive file, calls the JS callback to notify success.
  virtual void Run(DownloadStream *stream,
                   bool success,
                   const std::string &filename,
                   const std::string &mime_type)  {
    request_->FinishedCallback(stream, success, filename, mime_type);
  }

 private:
  ArchiveRequest::Ref request_;
  DISALLOW_COPY_AND_ASSIGN(ArchiveFinishedCallback);
};

// Sets up the parameters required for all ArchiveRequests.
void userglue_method_open(void *plugin_data,
                          ArchiveRequest *request,
                          const String &method,
                          const String &uri) {
  if (request->done()) {
    request->set_success(false);
    request->set_ready_state(ArchiveRequest::STATE_INIT);  // not ready
    return;  // We don't yet support reusing ArchiveRequests.
  }

  String method_lower(method);
  std::transform(method.begin(), method.end(), method_lower.begin(), ::tolower);
  if (method_lower != "get") {
    request->set_success(false);
    return;  // We don't yet support fetching files via POST.
  }
  request->set_uri(uri);
  request->set_ready_state(ArchiveRequest::STATE_OPEN);
}

// Starts progressively downloading the requested file
// The ArchiveRequest object will get callbacks as bytes stream in
void userglue_method_send(void *plugin_data,
                          ArchiveRequest *request) {
  PluginObject *plugin_object = static_cast<PluginObject *>(plugin_data);
  StreamManager *stream_manager = plugin_object->stream_manager();
  bool result = false;

  if (request->done()) {
    request->set_success(false);
    return;  // ArchiveRequests can't be reused.
  }
  if (request->ready_state() != 1) {  // Forgot to call open, or other error.
    request->set_success(false);
    return;
  }
  CHECK(request->pack());

  ArchiveNewStreamCallback *new_stream_callback =
      new ArchiveNewStreamCallback(request);

  ArchiveWriteReadyCallback *writeready_callback =
      new ArchiveWriteReadyCallback(request);

  ArchiveWriteCallback *write_callback =
      new ArchiveWriteCallback(request);

  ArchiveFinishedCallback *finished_callback =
      new ArchiveFinishedCallback(request);

  if (finished_callback) {
    DownloadStream *stream = stream_manager->LoadURL(request->uri(),
                                                     new_stream_callback,
                                                     writeready_callback,
                                                     write_callback,
                                                     finished_callback,
                                                     NP_NORMAL);
    if (!stream) {
      // We don't call O3D_ERROR here because the URI may be user set
      // so we don't want to cause an error callback when the developer
      // may not be able to know the URI is correct.
      request->set_error("could not create download stream");

      // We need to call the callback to report failure. Because it's async, the
      // code making the request can't know that once it has called send() that
      // the request still exists since send() may have called the callback and
      // the callback may have deleted the request.
      request->FinishedCallback(NULL, false, request->uri(), std::string(""));
    }

    // If stream is not NULL request may not exist as LoadURL may already have
    // completed and therefore called the callback which may have freed the
    // request so we can't set anything on the request here.
  }
}

}  // namespace class_ArchiveRequest
}  // namespace namespace_o3d
}  // namespace glue
