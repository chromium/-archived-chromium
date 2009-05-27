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
// TarGenerator generates a tar byte stream (uncompressed).
//
// A tar byte stream consists of a series of file headers, each followed by
// the actual file data.  Each file header starts on a block-aligned offset
// with the blocksize 512.  The start of data for each file is also
// block-aligned.  Zero-padding is added at the end of the file's data,
// if necessary to block-align...
//
// The normal usage is to call the AddFile() method for each file to add to the
// archive, then make one or more calls to AddFileBytes() to give the file's
// data.  Then repeat this sequence for each file to be added.  When done,
// call Finalize().

#ifndef O3D_IMPORT_CROSS_TAR_GENERATOR_H_
#define O3D_IMPORT_CROSS_TAR_GENERATOR_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "core/cross/types.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TarGenerator {
 public:
  explicit TarGenerator(StreamProcessor *callback_client)
      : callback_client_(callback_client),
        data_block_buffer_(TAR_BLOCK_SIZE),  // initialized to zeroes
        data_buffer_stream_(data_block_buffer_, TAR_BLOCK_SIZE) {}

  virtual ~TarGenerator() { Finalize(); }

  // Call AddFile() for each file entry, followed by calls to AddFileBytes()
  // for the file's data
  virtual void AddFile(const String &file_name,
                       size_t file_size);

  // Call to "push" bytes to be processed - our client will get called back
  // with the byte stream, with files rounded up to the nearest block size
  // (with zero padding)
  virtual int AddFileBytes(MemoryReadStream *stream, size_t n);

  // Must call this after all files and file data have been written
  virtual void Finalize();

 private:
  void AddEntry(const String &file_name,
                size_t file_size,
                bool is_directory);

  void AddDirectory(const String &file_name);
  void AddDirectoryEntryIfNeeded(const String &file_name);

  // Checksum for each header
  void ComputeCheckSum(uint8 *header);

  // flushes buffered file data to the client callback
  // if |flush_padding_zeroes| is |true| then flush a complete block
  // with zero padding even if less was buffered
  void FlushDataBuffer(bool flush_padding_zeroes);

  enum {TAR_HEADER_SIZE = 512};
  enum {TAR_BLOCK_SIZE = 512};

  StreamProcessor *callback_client_;

  // Buffers file data here - file data is in multiples of TAR_BLOCK_SIZE
  MemoryBuffer<uint8> data_block_buffer_;
  MemoryWriteStream data_buffer_stream_;

  // We use DirectoryMap to keep track of which directories we've already
  // written out headers for.  The client doesn't need to explicitly
  // add the directory entries.  Instead, the files will be stripped to their
  // base path and entries added for the directories as needed...
  struct StrCmp {
    bool operator()(const std::string &s1, const std::string &s2) const {
      return strcmp(s1.c_str(), s2.c_str()) < 0;
    }
  };

  typedef std::map<const std::string, bool, StrCmp>  DirectoryMap;

  DirectoryMap directory_map_;

  DISALLOW_COPY_AND_ASSIGN(TarGenerator);
};

}  // namespace o3d

#endif  //  O3D_IMPORT_CROSS_TAR_GENERATOR_H_
