// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mem_entry_impl.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/mem_backend_impl.h"

using base::Time;

namespace disk_cache {

MemEntryImpl::MemEntryImpl(MemBackendImpl* backend) {
  doomed_ = false;
  backend_ = backend;
  ref_count_ = 0;
  for (int i = 0; i < NUM_STREAMS; i++)
    data_size_[i] = 0;
}

MemEntryImpl::~MemEntryImpl() {
  for (int i = 0; i < NUM_STREAMS; i++)
    backend_->ModifyStorageSize(data_size_[i], 0);
  backend_->ModifyStorageSize(static_cast<int32>(key_.size()), 0);
}

bool MemEntryImpl::CreateEntry(const std::string& key) {
  key_ = key;
  last_modified_ = Time::Now();
  last_used_ = Time::Now();
  Open();
  backend_->ModifyStorageSize(0, static_cast<int32>(key.size()));
  return true;
}

void MemEntryImpl::Close() {
  ref_count_--;
  DCHECK(ref_count_ >= 0);
  if (!ref_count_ && doomed_)
    delete this;
}

void MemEntryImpl::Open() {
  ref_count_++;
  DCHECK(ref_count_ >= 0);
  DCHECK(!doomed_);
}

bool MemEntryImpl::InUse() {
  return ref_count_ > 0;
}

void MemEntryImpl::Doom() {
  if (doomed_)
    return;
  backend_->InternalDoomEntry(this);
}

void MemEntryImpl::InternalDoom() {
  doomed_ = true;
  if (!ref_count_)
    delete this;
}

std::string MemEntryImpl::GetKey() const {
  return key_;
}

Time MemEntryImpl::GetLastUsed() const {
  return last_used_;
}

Time MemEntryImpl::GetLastModified() const {
  return last_modified_;
}

int32 MemEntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= NUM_STREAMS)
    return 0;

  return data_size_[index];
}

int MemEntryImpl::ReadData(int index, int offset, net::IOBuffer* buf,
    int buf_len, net::CompletionCallback* completion_callback) {
  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = GetDataSize(index);
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (offset + buf_len > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  memcpy(buf->data() , &(data_[index])[offset], buf_len);
  return buf_len;
}

int MemEntryImpl::WriteData(int index, int offset, net::IOBuffer* buf,
    int buf_len, net::CompletionCallback* completion_callback, bool truncate) {
  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int max_file_size = backend_->MaxFileSize();

  // offset of buf_len could be negative numbers.
  if (offset > max_file_size || buf_len > max_file_size ||
      offset + buf_len > max_file_size) {
    int size = offset + buf_len;
    if (size <= max_file_size)
      size = kint32max;
    return net::ERR_FAILED;
  }

  // Read the size at this point.
  int entry_size = GetDataSize(index);

  PrepareTarget(index, offset, buf_len);

  if (entry_size < offset + buf_len) {
    backend_->ModifyStorageSize(entry_size, offset + buf_len);
    data_size_[index] = offset + buf_len;
  } else if (truncate) {
    if (entry_size > offset + buf_len) {
      backend_->ModifyStorageSize(entry_size, offset + buf_len);
      data_size_[index] = offset + buf_len;
    }
  }

  UpdateRank(true);

  if (!buf_len)
    return 0;

  memcpy(&(data_[index])[offset], buf->data(), buf_len);
  return buf_len;
}

void MemEntryImpl::PrepareTarget(int index, int offset, int buf_len) {
  int entry_size = GetDataSize(index);

  if (entry_size >= offset + buf_len)
    return;  // Not growing the stored data.

  if (static_cast<int>(data_[index].size()) < offset + buf_len)
    data_[index].resize(offset + buf_len);

  if (offset <= entry_size)
    return;  // There is no "hole" on the stored data.

  // Cleanup the hole not written by the user. The point is to avoid returning
  // random stuff later on.
  memset(&(data_[index])[entry_size], 0, offset - entry_size);
}

void MemEntryImpl::UpdateRank(bool modified) {
  Time current = Time::Now();
  last_used_ = current;

  if (modified)
    last_modified_ = current;

  if (!doomed_)
    backend_->UpdateRank(this);
}

}  // namespace disk_cache
