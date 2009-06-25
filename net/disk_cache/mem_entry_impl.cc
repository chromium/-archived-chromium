// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mem_entry_impl.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/mem_backend_impl.h"

using base::Time;

namespace {

const int kSparseData = 1;

// Maximum size of a sparse entry is 2 to the power of this number.
const int kMaxSparseEntryBits = 12;

// Sparse entry has maximum size of 4KB.
const int kMaxSparseEntrySize = 1 << kMaxSparseEntryBits;

// Convert global offset to child index.
inline int ToChildIndex(int64 offset) {
  return static_cast<int>(offset >> kMaxSparseEntryBits);
}

// Convert global offset to offset in child entry.
inline int ToChildOffset(int64 offset) {
  return static_cast<int>(offset & (kMaxSparseEntrySize - 1));
}

}  // nemespace

namespace disk_cache {

MemEntryImpl::MemEntryImpl(MemBackendImpl* backend) {
  doomed_ = false;
  backend_ = backend;
  ref_count_ = 0;
  parent_ = NULL;
  child_id_ = 0;
  child_first_pos_ = 0;
  next_ = NULL;
  prev_ = NULL;
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
  // Only a parent entry can be closed.
  DCHECK(type() == kParentEntry);
  ref_count_--;
  DCHECK(ref_count_ >= 0);
  if (!ref_count_ && doomed_)
    delete this;
}

void MemEntryImpl::Open() {
  // Only a parent entry can be opened.
  // TODO(hclam): make sure it's correct to not apply the concept of ref
  // counting to child entry.
  DCHECK(type() == kParentEntry);
  ref_count_++;
  DCHECK(ref_count_ >= 0);
  DCHECK(!doomed_);
}

bool MemEntryImpl::InUse() {
  if (type() == kParentEntry) {
    return ref_count_ > 0;
  } else {
    // A child entry is always not in use. The consequence is that a child entry
    // can always be evicted while the associated parent entry is currently in
    // used (i.e. opened).
    return false;
  }
}

void MemEntryImpl::Doom() {
  if (doomed_)
    return;
  if (type() == kParentEntry) {
    // Perform internal doom from the backend if this is a parent entry.
    backend_->InternalDoomEntry(this);
  } else {
    // Manually detach from the backend and perform internal doom.
    backend_->RemoveFromRankingList(this);
    InternalDoom();
  }
}

void MemEntryImpl::InternalDoom() {
  doomed_ = true;
  if (!ref_count_) {
    if (type() == kParentEntry) {
      // If this is a parent entry, we need to doom all the child entries.
      if (children_.get()) {
        EntryMap children;
        children.swap(*children_);
        for (EntryMap::iterator i = children.begin();
             i != children.end(); ++i) {
          // Since a pointer to this object is also saved in the map, avoid
          // dooming it.
          if (i->second != this)
            i->second->Doom();
        }
        DCHECK(children_->size() == 0);
      }
    } else {
      // If this is a child entry, detach it from the parent.
      parent_->DetachChild(child_id_);
    }
    delete this;
  }
}

std::string MemEntryImpl::GetKey() const {
  // A child entry doesn't have key so this method should not be called.
  DCHECK(type() == kParentEntry);
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
  DCHECK(type() == kParentEntry || index == kSparseData);

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
  DCHECK(type() == kParentEntry || index == kSparseData);

  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int max_file_size = backend_->MaxFileSize();

  // offset of buf_len could be negative numbers.
  if (offset > max_file_size || buf_len > max_file_size ||
      offset + buf_len > max_file_size) {
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

int MemEntryImpl::ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                                 net::CompletionCallback* completion_callback) {
  DCHECK(type() == kParentEntry);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0 || !buf_len)
    return net::ERR_INVALID_ARGUMENT;

  // We will keep using this buffer and adjust the offset in this buffer.
  scoped_refptr<net::ReusedIOBuffer> io_buf = new net::ReusedIOBuffer(buf,
                                                                      buf_len);

  // Counts the number of bytes read.
  int bytes_read = 0;

  // Iterate until we have read enough.
  while (bytes_read < buf_len) {
    MemEntryImpl* child = OpenChild(offset + bytes_read, false);

    // No child present for that offset.
    if (!child)
      break;

    // We then need to prepare the child offset and len.
    int child_offset = ToChildOffset(offset + bytes_read);

    // If we are trying to read from a position that the child entry has no data
    // we should stop.
    if (child_offset < child->child_first_pos_)
      break;
    int ret = child->ReadData(kSparseData, child_offset, io_buf,
                              buf_len - bytes_read, NULL);

    // If we encounter an error in one entry, return immediately.
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Increment the counter by number of bytes read in the child entry.
    bytes_read += ret;
    // And also adjust the buffer's offset.
    if (bytes_read < buf_len)
      io_buf->SetOffset(bytes_read);
  }

  UpdateRank(false);

  return bytes_read;
}

int MemEntryImpl::WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
    net::CompletionCallback* completion_callback) {
  DCHECK(type() == kParentEntry);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  scoped_refptr<net::ReusedIOBuffer> io_buf = new net::ReusedIOBuffer(buf,
                                                                      buf_len);
  // Counter for amount of bytes written.
  int bytes_written = 0;

  // This loop walks through child entries continuously starting from |offset|
  // and writes blocks of data (of maximum size kMaxSparseEntrySize) into each
  // child entry until all |buf_len| bytes are written. The write operation can
  // start in the middle of an entry.
  while (bytes_written < buf_len) {
    MemEntryImpl* child = OpenChild(offset + bytes_written, true);
    int child_offset = ToChildOffset(offset + bytes_written);

    // Find the right amount to write, this evaluates the remaining bytes to
    // write and remaining capacity of this child entry.
    int write_len = std::min(buf_len - bytes_written,
                             kMaxSparseEntrySize - child_offset);

    // Keep a record of the last byte position (exclusive) in the child.
    int data_size = child->GetDataSize(kSparseData);

    // Always writes to the child entry. This operation may overwrite data
    // previously written.
    // TODO(hclam): if there is data in the entry and this write is not
    // continuous we may want to discard this write.
    int ret = child->WriteData(kSparseData, child_offset, io_buf, write_len,
                               NULL, true);
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Keep a record of the first byte position in the child if the write was
    // not aligned nor continuous. This is to enable witting to the middle
    // of an entry and still keep track of data off the aligned edge.
    if (data_size != child_offset)
      child->child_first_pos_ = child_offset;

    // Increment the counter.
    bytes_written += ret;

    // And adjust the offset in the IO buffer.
    if (bytes_written < buf_len)
      io_buf->SetOffset(bytes_written);
  }

  UpdateRank(true);

  return bytes_written;
}

int MemEntryImpl::GetAvailableRange(int64 offset, int len, int64* start) {
  DCHECK(type() == kParentEntry);
  DCHECK(start);

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || len < 0 || !start)
    return net::ERR_INVALID_ARGUMENT;

  MemEntryImpl* current_child = NULL;

  // Find the first child and record the number of empty bytes.
  int empty = FindNextChild(offset, len, &current_child);
  if (current_child) {
    *start = offset + empty;
    len -= empty;

    // Counts the number of continuous bytes.
    int continuous = 0;

    // This loop scan for continuous bytes.
    while (len && current_child) {
      // Number of bytes available in this child.
      int data_size = current_child->GetDataSize(kSparseData) -
                      ToChildOffset(*start + continuous);
      if (data_size > len)
        data_size = len;

      // We have found more continuous bytes so increment the count. Also
      // decrement the length we should scan.
      continuous += data_size;
      len -= data_size;

      // If the next child is discontinuous, break the loop.
      if (FindNextChild(*start + continuous, len, &current_child))
        break;
    }
    return continuous;
  }
  *start = offset;
  return 0;
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

bool MemEntryImpl::InitSparseInfo() {
  DCHECK(type() == kParentEntry);

  if (!children_.get()) {
    // If we already have some data in sparse stream but we are being
    // initialized as a sparse entry, we should fail.
    if (GetDataSize(kSparseData))
      return false;
    children_.reset(new EntryMap());

    // The parent entry stores data for the first block, so save this object to
    // index 0.
    (*children_)[0] = this;
  }
  return true;
}

bool MemEntryImpl::InitChildEntry(MemEntryImpl* parent, int child_id) {
  DCHECK(!parent_);
  DCHECK(!child_id_);
  parent_ = parent;
  child_id_ = child_id;
  last_modified_ = Time::Now();
  last_used_ = Time::Now();
  // Insert this to the backend's ranking list.
  backend_->InsertIntoRankingList(this);
  return true;
}

MemEntryImpl* MemEntryImpl::OpenChild(int64 offset, bool create) {
  DCHECK(type() == kParentEntry);
  int index = ToChildIndex(offset);
  EntryMap::iterator i = children_->find(index);
  if (i != children_->end()) {
    return i->second;
  } else if (create) {
    MemEntryImpl* child = new MemEntryImpl(backend_);
    child->InitChildEntry(this, index);
    (*children_)[index] = child;
    return child;
  }
  return NULL;
}

int MemEntryImpl::FindNextChild(int64 offset, int len, MemEntryImpl** child) {
  DCHECK(child);
  *child = NULL;
  int scanned_len = 0;

  // This loop tries to find the first existing child.
  while (scanned_len < len) {
    // This points to the current offset in the child.
    int current_child_offset = ToChildOffset(offset + scanned_len);
    MemEntryImpl* current_child = OpenChild(offset + scanned_len, false);
    if (current_child) {
      int child_first_pos = current_child->child_first_pos_;

      // This points to the first byte that we should be reading from, we need
      // to take care of the filled region and the current offset in the child.
      int first_pos =  std::max(current_child_offset, child_first_pos);

      // If the first byte position we should read from doesn't exceed the
      // filled region, we have found the first child.
      if (first_pos < current_child->GetDataSize(kSparseData)) {
         *child = current_child;

         // We need to advance the scanned length.
         scanned_len += first_pos - current_child_offset;
         break;
      }
    }
    scanned_len += kMaxSparseEntrySize - current_child_offset;
  }
  return scanned_len;
}

void MemEntryImpl::DetachChild(int child_id) {
  children_->erase(child_id);
}

}  // namespace disk_cache
