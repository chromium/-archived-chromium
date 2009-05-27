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

//
// TarProcessor processes a tar byte stream (uncompressed).
//
// A tar byte stream consists of a series of file headers, each followed by
// the actual file data.  Each file header starts on a block-aligned offset
// with the blocksize 512.  The start of data for each file is also
// block-aligned.
//
// As a TarProcessor receives bytes, it calls the client
// callback method ReceiveFileHeader() when each complete file header has been
// received.  Then the client's ReceiveFileData() will be called (possibly
// repeatedly) as the file's data is received.  This is repeated until all of
// the files in the archive have been processed.

#ifndef O3D_IMPORT_CROSS_TAR_PROCESSOR_H_
#define O3D_IMPORT_CROSS_TAR_PROCESSOR_H_

#include "base/basictypes.h"
#include "import/cross/memory_stream.h"
#include "import/cross/archive_processor.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TarProcessor : public StreamProcessor {
 public:
  explicit TarProcessor(ArchiveCallbackClient *callback_client)
      : callback_client_(callback_client),
        header_bytes_read_(0),
        file_bytes_to_read_(0) {}

  virtual ~TarProcessor() {}

  // Call to "push" bytes to be processed - the appropriate callback will get
  // called when we have enough data
  virtual int ProcessBytes(MemoryReadStream *stream, size_t n);

 private:
  enum {TAR_HEADER_SIZE = 512};
  enum {TAR_BLOCK_SIZE = 512};

  ArchiveCallbackClient  *callback_client_;
  size_t                  header_bytes_read_;
  char                   header_[TAR_HEADER_SIZE];

  // Initialized to total number of file bytes,
  // including zero padding up to block size
  // We read this many bytes to get to the next header
  size_t                 file_bytes_to_read_;

  // Initialized to the actual file size (not counting zero-padding) - keeps
  // track of number of bytes the client needs to read for the current file
  size_t                 client_file_bytes_to_read_;

  DISALLOW_COPY_AND_ASSIGN(TarProcessor);
};

}  // namespace o3d

#endif  //  O3D_IMPORT_CROSS_TAR_PROCESSOR_H_
