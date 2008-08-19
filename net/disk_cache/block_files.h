// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCK_FILES_H__
#define NET_DISK_CACHE_BLOCK_FILES_H__

#include <vector>

#include "net/disk_cache/addr.h"
#include "net/disk_cache/mapped_file.h"

namespace disk_cache {

class EntryImpl;

// This class handles the set of block-files open by the disk cache.
class BlockFiles {
 public:
  explicit BlockFiles(const std::wstring& path)
      : init_(false), zero_buffer_(NULL), path_(path) {}
  ~BlockFiles();

  // Performs the object initialization. create_files indicates if the backing
  // files should be created or just open.
  bool Init(bool create_files);

  // Returns the file that stores a given address.
  MappedFile* GetFile(Addr address);

  // Creates a new entry on a block file. block_type indicates the size of block
  // to be used (as defined on cache_addr.h), block_count is the number of
  // blocks to allocate, and block_address is the address of the new entry.
  bool CreateBlock(FileType block_type, int block_count, Addr* block_address);

  // Removes an entry from the block files. If deep is true, the storage is zero
  // filled; otherwise the entry is removed but the data is not altered (must be
  // already zeroed).
  void DeleteBlock(Addr address, bool deep);

  // Close all the files and set the internal state to be initializad again. The
  // cache is being purged.
  void CloseFiles();

 private:
  // Set force to true to overwrite the file if it exists.
  bool CreateBlockFile(int index, FileType file_type, bool force);
  bool OpenBlockFile(int index);

  // Attemp to grow this file. Fails if the file cannot be extended anymore.
  bool GrowBlockFile(MappedFile* file, BlockFileHeader* header);

  // Returns the appropriate file to use for a new block.
  MappedFile* FileForNewBlock(FileType block_type, int block_count);

  // Returns the next block file on this chain, creating new files if needed.
  MappedFile* NextFile(const MappedFile* file);

  // Creates an empty block file and returns its index.
  int CreateNextBlockFile(FileType block_type);

  // Restores the header of a potentially inconsistent file.
  bool FixBlockFileHeader(MappedFile* file);

  // Returns the filename for a given file index.
  std::wstring Name(int index);

  bool init_;
  char* zero_buffer_;  // Buffer to speed-up cleaning deleted entries.
  std::wstring path_;  // Path to the backing folder.
  std::vector<MappedFile*> block_files_;  // The actual files.

  DISALLOW_EVIL_CONSTRUCTORS(BlockFiles);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCK_FILES_H__
