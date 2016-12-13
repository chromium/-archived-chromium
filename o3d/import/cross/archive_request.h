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


// This file contains the declaration of the ArchiveRequest class.

#ifndef O3D_IMPORT_CROSS_ARCHIVE_REQUEST_H_
#define O3D_IMPORT_CROSS_ARCHIVE_REQUEST_H_

#include <algorithm>
#include <string>

#include "base/scoped_ptr.h"
#include "core/cross/callback.h"
#include "core/cross/object_base.h"
#include "core/cross/pack.h"
#include "import/cross/targz_processor.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "import/cross/raw_data.h"
#include "plugin/cross/download_stream.h"

namespace o3d {

typedef Closure ArchiveRequestCallback;

// An ArchiveRequest object is used to carry out an asynchronous request
// for a file to be loaded.
class ArchiveRequest : public ObjectBase, public ArchiveCallbackClient {
 public:
  enum ReadyState {  // These are copied from XMLHttpRequest.
    STATE_INIT = 0,
    STATE_OPEN = 1,
    STATE_SENT = 2,
    STATE_RECEIVING = 3,
    STATE_LOADED = 4,
  };

  // A file by this name must be the first file in the archive otherwise
  // the archive is rejected. This is a security measure so that O3D
  // can not be used to open arbitrary .tgz files but only those files someone
  // has specifically prepared for O3D. This file will not be passed to
  // the onfileavailable callback.
  static const char* O3D_MARKER;

  // The contents of the O3D_MARKER file. Arguably the content should not matter
  // but for the sake of completeness we define the content so there is no
  // ambiguity.
  static const char* O3D_MARKER_CONTENT;

  // The size of the O3D_MARKER_CONTENT.
  static const size_t O3D_MARKER_CONTENT_LENGTH;

 public:
  typedef SmartPointer<ArchiveRequest> Ref;

  virtual ~ArchiveRequest();

  static ArchiveRequest *Create(ServiceLocator* service_locator,
                                Pack *pack);


  // Streaming callbacks
  virtual void NewStreamCallback(glue::DownloadStream *stream);
  virtual int32 WriteReadyCallback(glue::DownloadStream *stream);
  virtual int32 WriteCallback(glue::DownloadStream *stream,
                              int32 offset,
                              int32 length,
                              void *data);

  virtual void FinishedCallback(glue::DownloadStream *stream,
                                bool success,
                                const std::string &filename,
                                const std::string &mime_type);

  // ArchiveCallbackClient methods
  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info);
  virtual bool ReceiveFileData(MemoryReadStream *stream, size_t nbytes);

  Pack *pack() {
    return pack_.Get();  // Set at creation time and never changed.
  }

  ArchiveRequestCallback *onfileavailable() {
    return onfileavailable_.get();
  }
  void set_onfileavailable(ArchiveRequestCallback *onfileavailable) {
    onfileavailable_.reset(onfileavailable);
  }

  ArchiveRequestCallback *onreadystatechange() {
    return onreadystatechange_.get();
  }
  void set_onreadystatechange(ArchiveRequestCallback *onreadystatechange) {
    onreadystatechange_.reset(onreadystatechange);
  }

  // returns the "current" data object (used by the callback)
  RawData *data() {
    return raw_data_.Get();
  }

  const String& uri() {
    return uri_;
  }
  void set_uri(const String& uri) {
    uri_ = uri;
  }

  bool done() {
    return done_;
  }

  bool success() {
    return success_;
  }
  void set_success(bool success) {
    success_ = success;
    done_ = true;
    pack_.Reset(); // Remove pack reference to allow garbage collection of pack.
  }

  const String& error() const {
    return error_;
  }
  void set_error(const String& error) {
    error_ = error;
  }

  int ready_state() {
    return ready_state_;
  }
  void set_ready_state(int state) {
    ready_state_ = state;
  }

  int stream_length() {
    return stream_length_;
  }

  int bytes_received() {
    return bytes_received_;
  }

 protected:
  ArchiveRequest(ServiceLocator* service_locator, Pack *pack);

  Pack::Ref pack_;
  scoped_ptr<ArchiveRequestCallback> onreadystatechange_;
  scoped_ptr<ArchiveRequestCallback> onfileavailable_;
  String uri_;

  // Request state
  bool done_;  // Set after completion/failure to indicate success_ is valid.
  bool success_;  // Set after completion/failure to indicate which it is.
  int ready_state_;  // Like the XMLHttpRequest variable of the same name.
  String error_;  // Set after completion on failure.

  TarGzProcessor      *archive_processor_;
  std::vector<RawData::Ref> raw_data_list_;
  RawData::Ref        raw_data_;
  MemoryBuffer<uint8> temp_buffer_;
  MemoryWriteStream   file_memory_stream_;
  String              current_filename_;

  int                 stream_length_;   // total length of stream
  int                 bytes_received_;  // bytes received so far

  O3D_DECL_CLASS(ArchiveRequest, ObjectBase)
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArchiveRequest);
};  // ArchiveRequest

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_ARCHIVE_REQUEST_H_
