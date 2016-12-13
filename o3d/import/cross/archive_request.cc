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

// This file contains the definition of the ArchiveRequest class.

#include "import/cross/precompile.h"

#include "import/cross/archive_request.h"

#include "import/cross/targz_processor.h"
#include "core/cross/pack.h"

#define DEBUG_ARCHIVE_CALLBACKS  0

using glue::DownloadStream;

namespace o3d {

O3D_DEFN_CLASS(ArchiveRequest, ObjectBase);

// NOTE: The file starts with "aaaaaaaa" in the hope that most tar.gz creation
// utilties can easily sort with this being the file first in the .tgz
// Otherwise you'll have to manually force it to be the first file.
const char* ArchiveRequest::O3D_MARKER = "aaaaaaaa.o3d";
const char* ArchiveRequest::O3D_MARKER_CONTENT = "o3d";
const size_t ArchiveRequest::O3D_MARKER_CONTENT_LENGTH = 3;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ArchiveRequest::ArchiveRequest(ServiceLocator* service_locator,
                               Pack *pack)
    : ObjectBase(service_locator),
      pack_(pack),
      done_(false),
      success_(false),
      ready_state_(0),
      stream_length_(0),
      bytes_received_(0) {
  archive_processor_ = new TarGzProcessor(this);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ArchiveRequest::~ArchiveRequest() {
  delete archive_processor_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ArchiveRequest *ArchiveRequest::Create(ServiceLocator* service_locator,
                                       Pack *pack) {
  ArchiveRequest *request = new ArchiveRequest(service_locator, pack);
  return request;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ArchiveRequest::NewStreamCallback(DownloadStream *stream) {
  // we're starting to stream - make note of the stream length
  stream_length_ = stream->GetStreamLength();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 ArchiveRequest::WriteReadyCallback(DownloadStream *stream) {
  // Setting this too high causes Firefox to timeout in the Write callback.
  return 1024;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 ArchiveRequest::WriteCallback(DownloadStream *stream,
                                    int32 offset,
                                    int32 length,
                                    void *data) {
  // Count the bytes as they stream in
  bytes_received_ += length;

  MemoryReadStream memory_stream(reinterpret_cast<uint8*>(data), length);

  // Progressively decompress the bytes we've just been given
  int result =
      archive_processor_->ProcessCompressedBytes(&memory_stream, length);

  if (result != Z_OK && result != Z_STREAM_END) {
    set_success(false);
    set_error("Invalid gzipped tar file");
    stream->Cancel();  // tell the browser to stop downloading
    // NOTE: Cancel will call NPP_Cancel which in turn will call
    // ArchiveRequest::FinishedCallback so we don't do anything here since
    // we may already have been deleted on return.
  }

  return length;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Loads the Archive file, calls the JS callback to notify success.
void ArchiveRequest::FinishedCallback(DownloadStream *stream,
                                      bool success,
                                      const std::string &filename,
                                      const std::string &mime_type)  {
  set_ready_state(ArchiveRequest::STATE_LOADED);

  // Since the standard codes only go far enough to tell us that the download
  // succeeded, we set the success [and implicitly the done] flags to give the
  // rest of the story.
  set_success(success);
  if (!success) {
    // I have no idea if an error is already set here but one MUST be set
    // so let's check.
    if (error().empty()) {
      set_error(String("Could not download archive: ") + uri());
    }
  }
  if (onreadystatechange())
    onreadystatechange()->Run();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ArchiveCallbackClient methods

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ArchiveRequest::ReceiveFileHeader(const ArchiveFileInfo &file_info) {
  int file_size = file_info.GetFileSize();

#if DEBUG_ARCHIVE_CALLBACKS
  printf("\n");
  printf("-----------------------------------------------------------------\n");
  printf("File Name: %s\n", file_info.GetFileName().c_str());
  printf("File Size: %d\n", file_size);
  printf("-----------------------------------------------------------------\n");
#endif

  if (file_size > 0) {  // skip over directory entries (with zero file size)
    // Save filename for when we create our RawData object
    current_filename_ = file_info.GetFileName();

    temp_buffer_.Allocate(file_size);
    file_memory_stream_.Assign(static_cast<uint8*>(temp_buffer_),
                               file_size);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ArchiveRequest::ReceiveFileData(MemoryReadStream *input_stream,
                                     size_t nbytes) {
  assert(input_stream->GetRemainingByteCount() >= nbytes);
  assert(file_memory_stream_.GetRemainingByteCount() >= nbytes);

  // Buffer file bytes from |input_stream| to |file_memory_stream_|
  file_memory_stream_.Write(input_stream->GetDirectMemoryPointer(), nbytes);
  input_stream->Skip(nbytes);

  // If we've just filled our file temp buffer then callback
  if (file_memory_stream_.GetRemainingByteCount() == 0) {
    // We've reached the end of file

    // Check if this is file metadata (extra file attributes) and skip if so
    // On the Mac, the tar command marks metadata by
    // pre-pending "._" to the filename
    bool is_metadata = false;
    std::string::size_type j = current_filename_.find("._");
    if (j != std::string::npos) {
      if (j == 0 || current_filename_[j - 1] == '/') is_metadata = true;
    }

    // Skip ".DS_Store" file which may be created in Mac-generated archives
    j = current_filename_.find(".DS_Store");
    if (j != std::string::npos) {
      if (j == 0 || current_filename_[j - 1] == '/') is_metadata = true;
    }

    if (!is_metadata && onfileavailable()) {
      // keep track of the "current" data object which the callback will use
      raw_data_ = RawData::Create(service_locator(),
                                  current_filename_,
                                  temp_buffer_,
                                  file_memory_stream_.GetTotalStreamLength() );

      // keeps them all around until the ArchiveRequest goes away
      raw_data_list_.push_back(raw_data_);

      // If it's the first file is must be the O3D_MARKER or else it's an error.
      if (raw_data_list_.size() == 1) {
        if (raw_data_->uri().compare(O3D_MARKER) != 0 ||
            raw_data_->StringValue().compare(O3D_MARKER_CONTENT) != 0) {
          set_error(String("Archive '")  + uri_ +
                    String("' is not intended for O3D. Missing '") +
                    O3D_MARKER + String("' as first file in archive."));
          return false;
        }
      } else {
        onfileavailable()->Run();
      }

      // If data hasn't been discarded (inside callback) then writes out to
      // temp file so we can get the data back at a later time
      raw_data_.Get()->Flush();

      // Remove the reference to the raw_data so we don't have undefined
      // behavior after the callback.
      raw_data_.Reset();
    }
  }
  return true;
}

}  // namespace o3d
