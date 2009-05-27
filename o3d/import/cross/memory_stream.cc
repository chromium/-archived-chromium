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
// memory buffers.

#include "import/cross/memory_stream.h"
#include "core/cross/types.h"

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
#error "IS_LITTLE_ENDIAN or IS_BIG_ENDIAN must be defined!"
#endif

namespace o3d {

namespace {
union Int16Union {
  uint8 c[2];
  int16 i;
};

union UInt16Union {
  uint8 c[2];
  uint16 i;
};

union Int32Union {
  uint8 c[4];
  int32 i;
};

union UInt32Union {
  uint8 c[4];
  uint32 i;
};

union Int32FloatUnion {
  int32 i;
  float f;
};

union UInt32FloatUnion {
  uint32 i;
  float f;
};
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static int16 SwapInt16(const int16 *value) {
  // endian-swap two bytes
  uint8 p[2];
  const char *q = reinterpret_cast<const char*>(value);
  p[0] = q[1];
  p[1] = q[0];

  return *reinterpret_cast<int16*>(p);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static uint16 SwapUInt16(const uint16 *value) {
  // endian-swap two bytes
  uint8 p[2];
  const char *q = reinterpret_cast<const char*>(value);
  p[0] = q[1];
  p[1] = q[0];

  return *reinterpret_cast<const uint16*>(p);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static int32 SwapInt32(const int32 *value) {
  // endian-swap four bytes
  char p[4];
  const char *q = reinterpret_cast<const char*>(value);
  p[0] = q[3];
  p[1] = q[2];
  p[2] = q[1];
  p[3] = q[0];

  return *reinterpret_cast<const int32*>(p);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static uint32 SwapUInt32(const uint32 *value) {
  // endian-swap four bytes
  char p[4];
  const char *q = reinterpret_cast<const char*>(value);
  p[0] = q[3];
  p[1] = q[2];
  p[2] = q[1];
  p[3] = q[0];

  return *reinterpret_cast<const uint32*>(p);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int16 MemoryReadStream::ReadLittleEndianInt16() {
  Int16Union u;
  Read(u.c, 2);

#ifdef IS_BIG_ENDIAN
  return SwapInt16(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint16 MemoryReadStream::ReadLittleEndianUInt16() {
  UInt16Union u;
  Read(u.c, 2);

#ifdef IS_BIG_ENDIAN
  return SwapUint16(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int16 MemoryReadStream::ReadBigEndianInt16() {
  Int16Union u;
  Read(u.c, 2);

#ifdef IS_LITTLE_ENDIAN
  return SwapInt16(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint16 MemoryReadStream::ReadBigEndianUInt16() {
  UInt16Union u;
  Read(u.c, 2);

#ifdef IS_LITTLE_ENDIAN
  return SwapUInt16(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 MemoryReadStream::ReadLittleEndianInt32() {
  Int32Union u;
  Read(u.c, 4);

#ifdef IS_BIG_ENDIAN
  return SwapInt32(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint32 MemoryReadStream::ReadLittleEndianUInt32() {
  UInt32Union u;
  Read(u.c, 4);

#ifdef IS_BIG_ENDIAN
  return SwapUInt32(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 MemoryReadStream::ReadBigEndianInt32() {
  Int32Union u;
  Read(u.c, 4);

#ifdef IS_LITTLE_ENDIAN
  return SwapInt32(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint32 MemoryReadStream::ReadBigEndianUInt32() {
  UInt32Union u;
  Read(u.c, 4);

#ifdef IS_LITTLE_ENDIAN
  return SwapUInt32(&u.i);
#else
  return u.i;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
float MemoryReadStream::ReadLittleEndianFloat32() {
  // Read in as int32 then interpret as float32
  Int32FloatUnion u;
  u.i = ReadLittleEndianInt32();
  return u.f;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
float MemoryReadStream::ReadBigEndianFloat32() {
  // Read in as int32 then interpret as float32
  Int32FloatUnion u;
  u.i = ReadBigEndianInt32();
  return u.f;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteLittleEndianInt16(int16 i) {
  Int16Union u;

#ifdef IS_BIG_ENDIAN
  u.i = SwapInt16(&i);
#else
  u.i = i;
#endif

  Write(u.c, 2);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteLittleEndianUInt16(uint16 i) {
  UInt16Union u;

#ifdef IS_BIG_ENDIAN
  u.i = SwapUInt16(&i);
#else
  u.i = i;
#endif

  Write(u.c, 2);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteBigEndianInt16(int16 i) {
  Int16Union u;

#ifdef IS_LITTLE_ENDIAN
  u.i = SwapInt16(&i);
#else
  u.i = i;
#endif

  Write(u.c, 2);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteBigEndianUInt16(uint16 i) {
  UInt16Union u;

#ifdef IS_LITTLE_ENDIAN
  u.i = SwapUInt16(&i);
#else
  u.i = i;
#endif

  Write(u.c, 2);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteLittleEndianInt32(int32 i) {
  Int32Union u;

#ifdef IS_BIG_ENDIAN
  u.i = SwapInt32(&i);
#else
  u.i = i;
#endif

  Write(u.c, 4);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteLittleEndianUInt32(uint32 i) {
  UInt32Union u;

#ifdef IS_BIG_ENDIAN
  u.i = SwapUInt32(&i);
#else
  u.i = i;
#endif

  Write(u.c, 4);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteBigEndianInt32(int32 i) {
  Int32Union u;

#ifdef IS_LITTLE_ENDIAN
  u.i = SwapInt32(&i);
#else
  u.i = i;
#endif

  Write(u.c, 4);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteBigEndianUInt32(uint32 i) {
  UInt32Union u;

#ifdef IS_LITTLE_ENDIAN
  u.i = SwapUInt32(&i);
#else
  u.i = i;
#endif

  Write(u.c, 4);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteLittleEndianFloat32(float f) {
  // Interpret byte-pattern of f as int32 and write out
  Int32FloatUnion u;
  u.f = f;
  WriteLittleEndianInt32(u.i);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MemoryWriteStream::WriteBigEndianFloat32(float f) {
  // Interpret byte-pattern of f as int32 and write out
  Int32FloatUnion u;
  u.f = f;
  WriteBigEndianInt32(u.i);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// A couple of useful utility functions

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the bytes pointed to by |value| as little-endian 16
int16 MemoryReadStream::GetLittleEndianInt16(const int16 *value) {
#ifdef IS_BIG_ENDIAN
  return SwapInt16(value);
#else
  return *value;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the bytes pointed to by |value| as little-endian 16
uint16 MemoryReadStream::GetLittleEndianUInt16(const uint16 *value) {
#ifdef IS_BIG_ENDIAN
  return SwapUInt16(value);
#else
  return *value;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the bytes pointed to by |value| as little-endian 32
int32 MemoryReadStream::GetLittleEndianInt32(const int32 *value) {
#ifdef IS_BIG_ENDIAN
  return SwapInt32(value);
#else
  return *value;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the bytes pointed to by |value| as little-endian 32
uint32 MemoryReadStream::GetLittleEndianUInt32(const uint32 *value) {
#ifdef IS_BIG_ENDIAN
  return SwapUint32(value);
#else
  return *value;
#endif
}

}  // namespace o3d
