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

#include "base/logging.h"
#include "import/cross/tar_processor.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int TarProcessor::ProcessBytes(MemoryReadStream *stream, size_t n) {
  // Keep processing the byte-stream until we've consumed all we're given
  //
  size_t bytes_to_consume = n;

  while (bytes_to_consume > 0) {
    // First see if we have any more header bytes to read
    if (header_bytes_read_ < TAR_HEADER_SIZE) {
      // Read header bytes
      size_t header_bytes_remaining = TAR_HEADER_SIZE - header_bytes_read_;
      size_t bytes_to_read = std::min(bytes_to_consume, header_bytes_remaining);
      size_t bytes_read =
          stream->Read(reinterpret_cast<uint8*>(header_ + header_bytes_read_),
                       bytes_to_read);
      if (bytes_read != bytes_to_read) {
        return -1;
      }

      header_bytes_read_ += bytes_to_read;
      bytes_to_consume -= bytes_to_read;

      if (header_bytes_read_ == TAR_HEADER_SIZE) {
        const char *filename = (const char *)header_;

        // The tar format stupidly represents size_teger values as
        // octal strings!!
        size_t file_size = 0;
        sscanf(header_ + 124, "%o", &file_size);

        // Only callback client if this is a "real" header
        // (filename is not NULL)
        // Extra zero-padding can be added by the gzip compression
        // (at end of archive), so ignore these ones.
        //
        // Also, ignore entries for directories (which have zero size)
        if (header_[0] != 0 && file_size > 0) {
          ArchiveFileInfo info(filename, file_size);
          callback_client_->ReceiveFileHeader(info);
        } else if (header_[0] == 0) {
          // If filename is NULL due to zero-padding then file size
          // should also be NULL
          assert(file_size == 0);
        }

        // Round filesize up to nearest block size
        file_bytes_to_read_ =
            (file_size + TAR_BLOCK_SIZE - 1) & ~(TAR_BLOCK_SIZE - 1);

        // Our client doesn't want to be bothered with the block padding,
        // so only send him the actual file bytes
        client_file_bytes_to_read_ = file_size;
      }
    }

    if (bytes_to_consume > 0) {
      // Looks like we have some left-over bytes past the header
      // -- size_terpret as file bytes
      if (client_file_bytes_to_read_ > 0) {
        // Callback to client with some file data

        // Use a copy of the stream in case the client doesn't read
        // the amount they're supposed to
        MemoryReadStream client_read_stream(*stream);

        size_t client_bytes_this_time =
            std::min(bytes_to_consume, client_file_bytes_to_read_);

        if (!callback_client_->ReceiveFileData(&client_read_stream,
                                               client_bytes_this_time)) {
          return -1;
        }

        client_file_bytes_to_read_ -= client_bytes_this_time;
        file_bytes_to_read_ -= client_bytes_this_time;

        // Advance stream the amount the client should have consumed
        stream->Skip(client_bytes_this_time);

        bytes_to_consume -= client_bytes_this_time;
      }

      // Now, check if we have any padding bytes (up to block size)
      // past the file data
      if (bytes_to_consume > 0) {
        size_t bytes_to_skip = std::min(bytes_to_consume, file_bytes_to_read_);
        stream->Skip(bytes_to_skip);
        file_bytes_to_read_ -= bytes_to_skip;
        bytes_to_consume -= bytes_to_skip;
      }

      if (file_bytes_to_read_ == 0) {
        // We've read all of the file data,
        // so now expect the next file header

        // setting to 0 makes us want more header bytes
        header_bytes_read_ = 0;
      }
    }
  }

  return 0;
}

}  // namespace o3d
