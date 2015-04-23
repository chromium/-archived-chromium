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


// This file contains the declaration of class IArchiveGenerator.

#ifndef O3D_IMPORT_CROSS_IARCHIVE_GENERATOR_H_
#define O3D_IMPORT_CROSS_IARCHIVE_GENERATOR_H_

#include "core/cross/types.h"

namespace o3d {

class MemoryReadStream;

class IArchiveGenerator {
 public:
  virtual ~IArchiveGenerator() {
  }

  // Call AddFile() for each file entry, followed by calls to AddFileBytes()
  // for the file's data
  virtual void AddFile(const String& file_name,
                       size_t file_size) = 0;

  // Call with the file's data (after calling AddFile)
  // may be called one time will all the file's data, or multiple times
  // until all the data is provided
  virtual int AddFileBytes(MemoryReadStream* stream, size_t n) = 0;
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_IARCHIVE_GENERATOR_H_
