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
// MemoryReadStream and MemoryWriteStream are simple stream wrappers around
// memory buffers.  Their constructers take a pointer to the memory and
// the length of the buffer.  They are useful for pipeline based processing
// of byte-streams.
//
// MemoryReadStream maintains its stream position
// and can be read with the Read() method, returning the number of bytes
// read (similar to fread()).
//
// MemoryWriteStream maintains its stream position
// and can be written with the Write() method, returning the number of bytes
// actually written (similar to fwrite()).

#ifndef O3D_IMPORT_CROSS_MEMORY_STREAM_H_
#define O3D_IMPORT_CROSS_MEMORY_STREAM_H_

#include <string.h>
#include "base/basictypes.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MemoryReadStream {
 public:
  MemoryReadStream(const uint8 *memory, size_t n)
      : memory_(memory), length_(n), read_index_(0) {}

  // Explicit copy constructor
  MemoryReadStream(const MemoryReadStream &stream)
      : memory_(stream.memory_),
        length_(stream.length_),
        read_index_(stream.read_index_) {}

  virtual ~MemoryReadStream() {}

  // Similar to fread(), will try to copy |n| bytes to address |p|
  // (copies fewer bytes if the stream doesn't have enough data)
  // returns the number of bytes copied
  virtual size_t Read(void *p, size_t n) {
    size_t remaining = GetRemainingByteCount();
    size_t bytes_to_read = (n < remaining) ? n : remaining;
    memcpy(p, memory_ + read_index_, bytes_to_read);
    read_index_ += bytes_to_read;
    return bytes_to_read;
  }

  // Attempts to read a complete object of type T
  // returns |true| on success
  template<typename T>
  bool ReadAs(T *p) {
    size_t bytes_read = this->Read(reinterpret_cast<char*>(p),
                                   sizeof(T));
    return (bytes_read == sizeof(T));
  }

  // Reads the next byte in the stream (if there are any left)
  // If the stream has run dry (is empty) then returns 0
  // To check number of bytes available, call GetRemainingByteCount()
  virtual uint8 ReadByte() {
    if (read_index_ >= length_) return 0;

    uint8 byte = memory_[read_index_];
    read_index_++;

    return byte;
  }

  // 16 and 32bit integer reading for both little and big endian
  int16 ReadLittleEndianInt16();
  uint16 ReadLittleEndianUInt16();
  int16 ReadBigEndianInt16();
  uint16 ReadBigEndianUInt16();
  int32 ReadLittleEndianInt32();
  uint32 ReadLittleEndianUInt32();
  int32 ReadBigEndianInt32();
  uint32 ReadBigEndianUInt32();

  // IEEE 32-bit float reading (little and big endian)
  float ReadLittleEndianFloat32();
  float ReadBigEndianFloat32();

  // Returns the number of bytes left in the stream (which can be read)
  size_t GetRemainingByteCount() {
    return length_ - read_index_;
  };

  // Returns |true| if the read position is at end-of-stream
  // (no more bytes left to read)
  bool EndOfStream() { return GetRemainingByteCount() == 0; }

  // Instead of calling Read(), this gives direct access to the data
  // without a memcpy().
  // Calling GetRemainingByteCount() will give the number of remaining bytes
  // starting at this address...
  const uint8 *GetDirectMemoryPointer() { return memory_ + read_index_; }

  // Same as GetDirectMemoryPointer() but returns pointer of desired type
  template <typename T>
  T *GetDirectMemoryPointerAs() {
    return reinterpret_cast<T*>(GetDirectMemoryPointer());
  };

  // Advances the read position by |n| bytes
  void Skip(size_t n) {
    size_t remaining = GetRemainingByteCount();
    read_index_ += (n < remaining) ? n : remaining;
  }

  // Changes the read position to the given byte offset.
  // This allows random access into the stream similar to fseek()
  bool Seek(size_t seek_pos) {
    bool valid = (seek_pos >= 0 && seek_pos <= length_);
    if (valid) {
      read_index_ = seek_pos;
    }
    return valid;
  }

  // Returns the total number of bytes in the stream
  size_t GetTotalStreamLength() const { return length_; }


  // Returns the byte position (offset from start of stream)
  // this is effectively the number of bytes which have, so far,
  // been read
  size_t GetStreamPosition() const { return read_index_; }

  // utility methods (swaps the value if necessary)
  static int16 GetLittleEndianInt16(const int16 *value);
  static uint16 GetLittleEndianUInt16(const uint16 *value);
  static int32 GetLittleEndianInt32(const int32 *value);
  static uint32 GetLittleEndianUInt32(const uint32 *value);

 protected:
  const uint8 *memory_;
  size_t read_index_;
  size_t length_;

  // Disallow assignment
  void operator=(const MemoryReadStream&);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MemoryWriteStream {
 public:
  // May be completely initialized by calling Assign()
  MemoryWriteStream() : memory_(NULL), write_index_(0), length_(0) {}

  MemoryWriteStream(uint8 *memory, size_t n)
      : memory_(memory), length_(n), write_index_(0) {}

  // Explicit copy constructor
  MemoryWriteStream(const MemoryWriteStream &stream)
      : memory_(stream.memory_),
        length_(stream.length_),
        write_index_(stream.write_index_) {}

  virtual ~MemoryWriteStream() {}

  // In the case where the default constructor was used, this is useful
  // to assign a pointer to memory and a length in bytes.
  void Assign(uint8 *memory, size_t n) {
    memory_ = memory;
    length_ = n;
    write_index_ = 0;
  }

  // Changes the write position to the given byte offset.
  // This allows random access into the stream similar to fseek()
  bool Seek(size_t seek_pos) {
    bool valid = (seek_pos >= 0 && seek_pos <= length_);
    if (valid) {
      write_index_ = seek_pos;
    }
    return valid;
  }

  // Similar to fwrite(), will try to copy |n| bytes from address |p|
  // (copies fewer bytes if the stream doesn't have enough data)
  // returns the number of bytes copied
  virtual size_t Write(const void *p, size_t n) {
    size_t remaining = GetRemainingByteCount();
    size_t bytes_to_write = (n < remaining) ? n : remaining;
    memcpy(memory_ + write_index_, p, bytes_to_write);
    write_index_ += bytes_to_write;
    return bytes_to_write;
  }

  void WriteByte(uint8 byte) {
    Write(&byte, 1);
  }

  // 16 and 32bit integer writing for both little and big endian
  void WriteLittleEndianInt16(int16 i);
  void WriteLittleEndianUInt16(uint16 i);
  void WriteBigEndianInt16(int16 i);
  void WriteBigEndianUInt16(uint16 i);
  void WriteLittleEndianInt32(int32 i);
  void WriteLittleEndianUInt32(uint32 i);
  void WriteBigEndianInt32(int32 i);
  void WriteBigEndianUInt32(uint32 i);

  // IEEE 32-bit float writing (little and big endian)
  void WriteLittleEndianFloat32(float f);
  void WriteBigEndianFloat32(float f);

  // Returns the number of bytes left in the stream (which can be written)
  size_t GetRemainingByteCount() {
    return length_ - write_index_;
  };

  // Returns |true| if the read position is at end-of-stream
  // (no more bytes left to read)
  bool EndOfStream() { return GetRemainingByteCount() == 0; }

  // Returns the total number of bytes in the stream
  size_t GetTotalStreamLength() const { return length_; }

  // Returns the byte position (offset from start of stream)
  // this is effectively the number of bytes which have, so far,
  // been written
  size_t GetStreamPosition() const { return write_index_; }

 protected:
  uint8 *memory_;
  size_t write_index_;
  size_t length_;

  // Disallow assignment
  void operator=(const MemoryWriteStream&);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Abstract interface to process a memory stream
class StreamProcessor {
 public:
  virtual ~StreamProcessor() {}
  virtual int ProcessBytes(MemoryReadStream *stream,
                           size_t bytes_to_process) = 0;
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_MEMORY_STREAM_H_
