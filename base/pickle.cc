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

#include <stdlib.h>
#include <string>

#include "base/pickle.h"

//------------------------------------------------------------------------------

// static
const int Pickle::kPayloadUnit = 64;

// Payload is uint32 aligned.

Pickle::Pickle()
    : header_(NULL),
      header_size_(sizeof(Header)),
      capacity_(0),
      variable_buffer_offset_(0) {
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(int header_size)
    : header_(NULL),
      header_size_(AlignInt(header_size, sizeof(uint32))),
      capacity_(0),
      variable_buffer_offset_(0) {
  DCHECK(header_size >= sizeof(Header));
  DCHECK(header_size <= kPayloadUnit);
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(const char* data, int data_len)
    : header_(reinterpret_cast<Header*>(const_cast<char*>(data))),
      header_size_(data_len - header_->payload_size),
      capacity_(-1),
      variable_buffer_offset_(0) {
  DCHECK(header_size_ >= sizeof(Header));
  DCHECK(header_size_ == AlignInt(header_size_, sizeof(uint32)));
}

Pickle::Pickle(const Pickle& other)
    : header_(NULL),
      header_size_(other.header_size_),
      capacity_(0),
      variable_buffer_offset_(other.variable_buffer_offset_) {
  size_t payload_size = header_size_ + other.header_->payload_size;
  bool resized = Resize(payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_, payload_size);
}

Pickle::~Pickle() {
  if (capacity_ != -1)
    free(header_);
}

Pickle& Pickle::operator=(const Pickle& other) {
  if (header_size_ != other.header_size_ && capacity_ != -1) {
    free(header_);
    header_ = NULL;
    header_size_ = other.header_size_;
  }
  bool resized = Resize(other.header_size_ + other.header_->payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_, header_size_ + other.header_->payload_size);
  variable_buffer_offset_ = other.variable_buffer_offset_;
  return *this;
}

bool Pickle::ReadBool(void** iter, bool* result) const {
  DCHECK(iter);

  int tmp;
  if (!ReadInt(iter, &tmp))
    return false;
  DCHECK(0 == tmp || 1 == tmp);
  *result = tmp ? true : false;
  return true;
}

bool Pickle::ReadInt(void** iter, int* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  // Next line is otherwise the same as: memcpy(result, *iter, sizeof(*result));
  *result = *reinterpret_cast<int*>(*iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadLength(void** iter, int* result) const {
  if (!ReadInt(iter, result))
    return false;
  return ((*result) >= 0);
}

bool Pickle::ReadSize(void** iter, size_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  // TODO(jar) bug 1129285: Pickle should be cleaned up, and not dependent on
  // alignment.
  // Next line is otherwise the same as: memcpy(result, *iter, sizeof(*result));
  *result = *reinterpret_cast<size_t*>(*iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt64(void** iter, int64* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadIntPtr(void** iter, intptr_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  memcpy(result, *iter, sizeof(*result));

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadString(void** iter, std::string* result) const {
  DCHECK(iter);

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len))
    return false;

  char* chars = reinterpret_cast<char*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len);
  return true;
}

bool Pickle::ReadWString(void** iter, std::wstring* result) const {
  DCHECK(iter);

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len * sizeof(wchar_t)))
    return false;

  wchar_t* chars = reinterpret_cast<wchar_t*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len * sizeof(wchar_t));
  return true;
}

bool Pickle::ReadBytes(void** iter, const char** data, int length) const {
  DCHECK(iter);
  DCHECK(data);

  if (!IteratorHasRoomFor(*iter, length))
    return false;

  *data = reinterpret_cast<const char*>(*iter);

  UpdateIter(iter, length);
  return true;
}

bool Pickle::ReadData(void** iter, const char** data, int* length) const {
  DCHECK(iter);
  DCHECK(data);
  DCHECK(length);

  if (!ReadLength(iter, length))
    return false;

  return ReadBytes(iter, data, *length);
}

char* Pickle::BeginWrite(size_t length) {
  // write at a uint32-aligned offset from the beginning of the header
  size_t offset = AlignInt(header_->payload_size, sizeof(uint32));

  size_t new_size = offset + length;
  if (header_size_ + new_size > capacity_ && !Resize(header_size_ + new_size))
    return NULL;

  header_->payload_size = new_size;
  return payload() + offset;
}

void Pickle::EndWrite(char* dest, int length) {
  // Zero-pad to keep tools like purify from complaining about uninitialized
  // memory.
  if (length % sizeof(uint32))
    memset(dest + length, 0, sizeof(uint32) - (length % sizeof(uint32)));
}

bool Pickle::WriteBytes(const void* data, int data_len) {
  DCHECK(capacity_ != -1) << "oops: pickle is readonly";

  char* dest = BeginWrite(data_len);
  if (!dest)
    return false;

  memcpy(dest, data, data_len);

  EndWrite(dest, data_len);
  return true;
}

bool Pickle::WriteString(const std::string& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<int>(value.size()));
}

bool Pickle::WriteWString(const std::wstring& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size() * sizeof(value.data()[0])));
}

bool Pickle::WriteData(const char* data, int length) {
  return WriteInt(length) && WriteBytes(data, length);
}

char* Pickle::BeginWriteData(int length) {
  DCHECK_EQ(variable_buffer_offset_, 0) <<
    "There can only be one variable buffer in a Pickle";

  if (!WriteInt(length))
    return false;

  char *data_ptr = BeginWrite(length);
  if (!data_ptr)
    return NULL;

  variable_buffer_offset_ =
      data_ptr - reinterpret_cast<char*>(header_) - sizeof(int);

  // EndWrite doesn't necessarily have to be called after the write operation,
  // so we call it here to pad out what the caller will eventually write.
  EndWrite(data_ptr, length);
  return data_ptr;
}

void Pickle::TrimWriteData(int length) {
  DCHECK(variable_buffer_offset_ != 0);

  VariableLengthBuffer *buffer = reinterpret_cast<VariableLengthBuffer*>(
      reinterpret_cast<char*>(header_) + variable_buffer_offset_);

  DCHECK_GE(buffer->length, length);

  int old_length = buffer->length;
  int trimmed_bytes = old_length - length;
  if (trimmed_bytes > 0) {
    header_->payload_size -= trimmed_bytes;
    buffer->length = length;
  }
}

bool Pickle::Resize(size_t new_capacity) {
  new_capacity = AlignInt(new_capacity, kPayloadUnit);

  void* p = realloc(header_, new_capacity);
  if (!p)
    return false;

  header_ = reinterpret_cast<Header*>(p);
  capacity_ = new_capacity;
  return true;
}

// static
const char* Pickle::FindNext(size_t header_size,
                             const char* start,
                             const char* end) {
  DCHECK(header_size == AlignInt(header_size, sizeof(uint32)));
  DCHECK(header_size <= kPayloadUnit);

  const Header* hdr = reinterpret_cast<const Header*>(start);
  const char* payload_base = start + header_size;
  const char* payload_end = payload_base + hdr->payload_size;
  if (payload_end < payload_base)
    return NULL;

  return (payload_end > end) ? NULL : payload_end;
}
