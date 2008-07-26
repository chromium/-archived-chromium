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

#ifndef NET_DISK_CACHE_CACHE_INTERNAL_INL_H__
#define NET_DISK_CACHE_CACHE_INTERNAL_INL_H__

#include "net/disk_cache/storage_block.h"

#include "net/disk_cache/trace.h"

namespace disk_cache {

template<typename T> StorageBlock<T>::StorageBlock(MappedFile* file,
                                                   Addr address)
    : file_(file), address_(address), data_(NULL), modified_(false),
      own_data_(false), extended_(false) {
  if (address.num_blocks() > 1)
    extended_ = true;
  DCHECK(!address.is_initialized() || sizeof(*data_) == address.BlockSize());
}

template<typename T> StorageBlock<T>::~StorageBlock() {
  if (modified_)
    Store();
  if (own_data_)
    delete data_;
}

template<typename T> void* StorageBlock<T>::buffer() const {
  return data_;
}

template<typename T> size_t StorageBlock<T>::size() const {
  if (!extended_)
    return sizeof(*data_);
  return address_.num_blocks() * sizeof(*data_);
}

template<typename T> DWORD StorageBlock<T>::offset() const {
  return address_.start_block() * address_.BlockSize();
}

template<typename T> bool StorageBlock<T>::LazyInit(MappedFile* file,
                                                    Addr address) {
  if (file_ || address_.is_initialized()) {
    NOTREACHED();
    return false;
  }
  file_ = file;
  address_.set_value(address.value());
  if (address.num_blocks() > 1)
    extended_ = true;

  DCHECK(sizeof(*data_) == address.BlockSize());
  return true;
}

template<typename T> void StorageBlock<T>::SetData(T* other) {
  DCHECK(!modified_);
  if (own_data_) {
    delete data_;
    own_data_ = false;
  }
  data_ = other;
}

template<typename T> void StorageBlock<T>::set_modified() {
  DCHECK(data_);
  modified_ = true;
}

template<typename T> T* StorageBlock<T>::Data() {
  if (!data_)
    AllocateData();
  return data_;
}

template<typename T> bool StorageBlock<T>::HasData() const {
  return (NULL != data_);
}

template<typename T> const Addr StorageBlock<T>::address() const {
  return address_;
}

template<typename T> bool StorageBlock<T>::Load() {
  if (file_) {
    if (!data_)
      AllocateData();

    if (file_->Load(this)) {
      modified_ = false;
      return true;
    }
  }
  LOG(WARNING) << "Failed data load.";
  Trace("Failed data load.");
  return false;
}

template<typename T> bool StorageBlock<T>::Store() {
  if (file_) {
    if (file_->Store(this)) {
      modified_ = false;
      return true;
    }
  }
  LOG(ERROR) << "Failed data store.";
  Trace("Failed data store.");
  return false;
}

template<typename T> void StorageBlock<T>::AllocateData() {
  DCHECK(!data_);
  if (!extended_) {
    data_ = new T;
  } else {
    void* buffer = new char[address_.num_blocks() * sizeof(*data_)];
    data_ = new(buffer) T;
  }
  own_data_ = true;
}

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_INTERNAL_INL_H__
