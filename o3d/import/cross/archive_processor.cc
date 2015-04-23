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


#include "import/cross/precompile.h"

#include <sys/stat.h>

#include "import/cross/archive_processor.h"

#include "base/logging.h"
#include "import/cross/memory_buffer.h"
#include "zlib.h"

const int kChunkSize = 16384;

namespace o3d {

#ifdef _DEBUG
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// For debugging only, report a zlib or i/o error
void zerr(int ret) {
  LOG(ERROR) << "ArchiveProcessor: ";

  switch (ret) {
    case Z_ERRNO:
      if (ferror(stdin))
        LOG(ERROR) << "error reading stdin\n";
      if (ferror(stdout))
        LOG(ERROR) << "error writing stdout\n";
      break;
    case Z_STREAM_ERROR:
      LOG(ERROR) << "invalid compression level\n";
      break;
    case Z_DATA_ERROR:
      LOG(ERROR) << "invalid or incomplete deflate data\n";
      break;
    case Z_MEM_ERROR:
      LOG(ERROR) << "out of memory\n";
      break;
    case Z_VERSION_ERROR:
      LOG(ERROR) << "zlib version mismatch!\n";
      break;
  }
}
#endif  // _DEBUG

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int ArchiveProcessor::ProcessEntireStream(MemoryReadStream *stream) {
  int result = Z_OK;
  bool has_error = false;

  // decompress until deflate stream ends or error
  do {
    int remaining = stream->GetRemainingByteCount();

    int process_this_time = remaining < kChunkSize ? remaining : kChunkSize;
    result = ProcessCompressedBytes(stream, process_this_time);

    has_error = (result != Z_OK && result != Z_STREAM_END);

#ifdef _DEBUG
    if (has_error) {
      zerr(result);
    }
#endif
  } while (result != Z_STREAM_END && !has_error);

  if (result == Z_STREAM_END) {
    // if we got to the end of stream, then we're good...
    result = Z_OK;
  }

  return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int ArchiveProcessor::ProcessFile(const char *filename) {
  struct stat file_info;
  int result = stat(filename, &file_info);
  if (result != 0) return -1;

  int file_length = file_info.st_size;
  if (file_length == 0) return -1;

  MemoryBuffer<uint8> buffer;
  buffer.Allocate(file_length);
  uint8 *p = buffer;

  // Test by reading in a tar.gz file and sending through the
  // progressive streaming system
  FILE *fp = fopen(filename, "rb");
  if (!fp) return -1;  // can't open file!
  fread(p, sizeof(uint8), file_length, fp);
  fclose(fp);

  MemoryReadStream stream(p, file_length);

  result = ProcessEntireStream(&stream);

  if (result == Z_STREAM_END) {
    // if we got to the end of stream, then we're good...
    result = Z_OK;
  }

  return result;
}

}  // namespace o3d
