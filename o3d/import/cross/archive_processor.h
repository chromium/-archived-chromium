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


#ifndef O3D_IMPORT_CROSS_ARCHIVE_PROCESSOR_H_
#define O3D_IMPORT_CROSS_ARCHIVE_PROCESSOR_H_

#include <string>
#include "base/basictypes.h"
#include "import/cross/memory_stream.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveFileInfo {
 public:
  ArchiveFileInfo(const std::string &filename, int file_size)
      : filename_(filename), file_size_(file_size) {}

  virtual ~ArchiveFileInfo() {}

  const std::string &GetFileName() const { return filename_; }
  int               GetFileSize() const { return file_size_; }

 private:
  std::string      filename_;
  int              file_size_;

  DISALLOW_COPY_AND_ASSIGN(ArchiveFileInfo);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveCallbackClient {
 public:
  virtual ~ArchiveCallbackClient() {}
  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info) = 0;
  virtual bool ReceiveFileData(MemoryReadStream *stream, size_t nbytes) = 0;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveProcessor {
 public:
  explicit ArchiveProcessor(ArchiveCallbackClient *callback_client) {}

  virtual ~ArchiveProcessor() {}

  // Call to "push" bytes into the processor.  They will be decompressed and
  // the appropriate callbacks on |callback_client| will happen
  // as files come in...
  //
  // Return values (using zlib error codes):
  // Z_OK         : Processing was successful - but not yet done
  // Z_STREAM_END : We're done - archive is completely/successfully processed
  // any other value indicates an error condition
  //
  // Note: even archive formats not based on zlib should use these codes
  // (Z_OK, Z_STREAM_END)
  //
  virtual int     ProcessCompressedBytes(MemoryReadStream *stream,
                                         size_t bytes_to_process) = 0;

  // Decompresses the complete file archive, making file callbacks as the files
  // come in...
  virtual int     ProcessFile(const char *filename);

  // Decompresses the complete archive from memory,
  // making file callbacks as the files come in...
  virtual int     ProcessEntireStream(MemoryReadStream *stream);

 protected:
  DISALLOW_COPY_AND_ASSIGN(ArchiveProcessor);
};

#ifdef _DEBUG
// For debugging, report a zlib or i/o error
extern void zerr(int result);
#endif
}  // namespace o3d
#endif  //  O3D_IMPORT_CROSS_ARCHIVE_PROCESSOR_H_
