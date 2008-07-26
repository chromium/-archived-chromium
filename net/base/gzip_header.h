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

// The GZipHeader class allows you to parse a gzip header, such as you
// might find at the beginning of a file compressed by gzip (ie, a .gz
// file), or at the beginning of an HTTP response that uses a gzip
// Content-Encoding. See RFC 1952 for the specification for the gzip
// header.
//
// The model is that you call ReadMore() for each chunk of bytes
// you've read from a file or socket.
//

#ifndef NET_BASE_GZIPHEADER_H__
#define NET_BASE_GZIPHEADER_H__

#include "base/basictypes.h"

class GZipHeader {
 public:
  GZipHeader() {
    Reset();
  }
  ~GZipHeader() {
  }

  // Wipe the slate clean and start from scratch.
  void Reset() {
    state_        = IN_HEADER_ID1;
    flags_        = 0;
    extra_length_ = 0;
  }

  enum Status {
    INCOMPLETE_HEADER,    // don't have all the bits yet...
    COMPLETE_HEADER,      // complete, valid header
    INVALID_HEADER,       // found something invalid in the header
  };

  // Attempt to parse the given buffer as the next installment of
  // bytes from a gzip header. If the bytes we've seen so far do not
  // yet constitute a complete gzip header, return
  // INCOMPLETE_HEADER. If these bytes do not constitute a *valid*
  // gzip header, return INVALID_HEADER. When we've seen a complete
  // gzip header, return COMPLETE_HEADER and set the pointer pointed
  // to by header_end to the first byte beyond the gzip header.
  Status ReadMore(const char* inbuf, int inbuf_len,
                  const char** header_end);
 private:

  static const uint8 magic[];  // gzip magic header

  enum {                       // flags (see RFC)
    FLAG_FTEXT        = 0x01,  // bit 0 set: file probably ascii text
    FLAG_FHCRC        = 0x02,  // bit 1 set: header CRC present
    FLAG_FEXTRA       = 0x04,  // bit 2 set: extra field present
    FLAG_FNAME        = 0x08,  // bit 3 set: original file name present
    FLAG_FCOMMENT     = 0x10,  // bit 4 set: file comment present
    FLAG_RESERVED     = 0xE0,  // bits 5..7: reserved
  };

  enum State {
    // The first 10 bytes are the fixed-size header:
    IN_HEADER_ID1,
    IN_HEADER_ID2,
    IN_HEADER_CM,
    IN_HEADER_FLG,
    IN_HEADER_MTIME_BYTE_0,
    IN_HEADER_MTIME_BYTE_1,
    IN_HEADER_MTIME_BYTE_2,
    IN_HEADER_MTIME_BYTE_3,
    IN_HEADER_XFL,
    IN_HEADER_OS,

    IN_XLEN_BYTE_0,
    IN_XLEN_BYTE_1,
    IN_FEXTRA,

    IN_FNAME,

    IN_FCOMMENT,

    IN_FHCRC_BYTE_0,
    IN_FHCRC_BYTE_1,

    IN_DONE,
  };

  int    state_;  // our current State in the parsing FSM: an int so we can ++
  uint8  flags_;         // the flags byte of the header ("FLG" in the RFC)
  uint16 extra_length_;  // how much of the "extra field" we have yet to read
};

#endif  // NET_BASE_GZIPHEADER_H__
