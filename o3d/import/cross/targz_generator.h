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
// TarGzGenerator generates a "tar.gz" byte stream.
// In other words, given a series of files, it will first create a tar archive
// from them, then apply gzip compression.
// This would be equivalent to using the "tar cf" command followed by "gzip"
//
// The normal usage is to call the AddFile() method for each file to add to the
// archive, then make one or more calls to AddFileBytes() to give the file's
// data.  Then repeat this sequence for each file to be added.  When done,
// call Finalize().

#ifndef O3D_IMPORT_CROSS_TARGZ_GENERATOR_H_
#define O3D_IMPORT_CROSS_TARGZ_GENERATOR_H_

#include <string>
#include "base/basictypes.h"
#include "core/cross/types.h"
#include "import/cross/gz_compressor.h"
#include "import/cross/iarchive_generator.h"
#include "import/cross/memory_stream.h"
#include "import/cross/tar_generator.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TarGzGenerator : public StreamProcessor, public IArchiveGenerator {
 public:
  // |callback_client| receives the tar.gz byte stream
  explicit TarGzGenerator(StreamProcessor *callback_client);

  // Call AddFile() for each file entry, followed by calls to AddFileBytes()
  // for the file's data
  void AddFile(const String &file_name,
               size_t file_size);

  // Call with the file's data (after calling AddFile)
  // may be called one time with all the file's data, or multiple times
  // until all the data is provided
  int AddFileBytes(MemoryReadStream *stream, size_t n);
  int AddFileBytes(const uint8 *data, size_t n);

  // Must call this after all files and file data have been written
  virtual void Finalize();

 private:
  // StreamProcessor method:
  // Receives the tar bytestream from the TarGenerator.
  // It then feeds this into the GzCompressor
  virtual int ProcessBytes(MemoryReadStream *stream,
                           size_t bytes_to_process);

  GzCompressor gz_compressor_;
  TarGenerator tar_generator_;

  DISALLOW_COPY_AND_ASSIGN(TarGzGenerator);
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_TARGZ_GENERATOR_H_
