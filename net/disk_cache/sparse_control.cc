// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sparse_control.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/entry_impl.h"

using base::Time;

namespace {

// Stream of the sparse data index.
const int kSparseIndex = 2;

// Stream of the sparse data.
const int kSparseData = 1;

}

namespace disk_cache {

SparseControl::~SparseControl() {
  if (child_)
    CloseChild();
  if (init_)
    WriteSparseData();
}

int SparseControl::Init() {
  DCHECK(!init_);

  // We should not have sparse data for the exposed entry.
  if (entry_->GetDataSize(kSparseData))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // Now see if there is something where we store our data.
  int rv = net::OK;
  int data_len = entry_->GetDataSize(kSparseIndex);
  if (!data_len) {
    rv = CreateSparseEntry();
  } else {
    rv = OpenSparseEntry(data_len);
  }

  if (rv == net::OK)
    init_ = true;
  return rv;
}

int SparseControl::StartIO(SparseOperation op, int64 offset, net::IOBuffer* buf,
                           int buf_len, net::CompletionCallback* callback) {
  DCHECK(init_);
  // We don't support simultaneous IO for sparse data.
  if (operation_ != kNoOperation)
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  // We only support up to 64 GB.
  if (offset + buf_len >= 0x1000000000LL)
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  DCHECK(!user_buf_);
  DCHECK(!user_callback_);

  // Copy the operation parameters.
  operation_ = op;
  offset_ = offset;
  user_buf_ = new net::ReusedIOBuffer(buf, buf_len);
  buf_len_ = buf_len;

  result_ = 0;
  pending_ = false;
  finished_ = false;
  user_callback_ = callback;

  DoChildrenIO();

  if (!pending_) {
    // Everything was done synchronously.
    operation_ = kNoOperation;
    user_buf_ = NULL;
    user_callback_ = NULL;
    return result_;
  }

  return net::ERR_IO_PENDING;
}

int SparseControl::GetAvailableRange(int64 offset, int len, int64* start) {
  DCHECK(init_);
  NOTIMPLEMENTED();
  return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
}

// We are going to start using this entry to store sparse data, so we have to
// initialize our control info.
int SparseControl::CreateSparseEntry() {
  // TODO(rvargas): Set/check a flag in EntryStore.

  memset(&sparse_header_, 0, sizeof(sparse_header_));
  sparse_header_.signature = Time::Now().ToInternalValue();
  sparse_header_.magic = kIndexMagic;
  sparse_header_.parent_key_len = entry_->GetKey().size();
  children_map_.Resize(kNumSparseBits, true);

  // Save the header. The bitmap is saved in the destructor.
  scoped_refptr<net::IOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&sparse_header_));

  int rv = entry_->WriteData(kSparseIndex, 0, buf, sizeof(sparse_header_), NULL,
                             false);
  if (rv != sizeof(sparse_header_)) {
    DLOG(ERROR) << "Unable to save sparse_header_";
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
  }
  return net::OK;
}

// We are opening an entry from disk. Make sure that our control data is there.
int SparseControl::OpenSparseEntry(int data_len) {
  if (data_len < static_cast<int>(sizeof(SparseData)))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (entry_->GetDataSize(kSparseData))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // TODO(rvargas): Set/check a flag in EntryStore.

  // Dont't go over board with the bitmap. 8 KB gives us offsets up to 64 GB.
  int map_len = data_len - sizeof(sparse_header_);
  if (map_len > 8 * 1024 || map_len % 4)
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  scoped_refptr<net::IOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&sparse_header_));

  // Read header.
  int rv = entry_->ReadData(kSparseIndex, 0, buf, sizeof(sparse_header_), NULL);
  if (rv != static_cast<int>(sizeof(sparse_header_)))
    return net::ERR_CACHE_READ_FAILURE;

  // The real validation should be performed by the caller. This is just to
  // double check.
  if (sparse_header_.magic != kIndexMagic ||
      sparse_header_.parent_key_len !=
          static_cast<int>(entry_->GetKey().size()))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // Read the actual bitmap.
  buf = new net::IOBuffer(map_len);
  rv = entry_->ReadData(kSparseIndex, sizeof(sparse_header_), buf, map_len,
                        NULL);
  if (rv != map_len)
    return net::ERR_CACHE_READ_FAILURE;

  // Grow the bitmap to the current size and copy the bits.
  children_map_.Resize(map_len * 8, false);
  children_map_.SetMap(reinterpret_cast<uint32*>(buf->data()), map_len);
  return net::OK;
}

bool SparseControl::OpenChild() {
  DCHECK_GE(result_, 0);

  std::string key = GenerateChildKey();
  if (child_) {
    // Keep using the same child or open another one?.
    if (key == child_->GetKey())
      return true;
    CloseChild();
  }

  // Se if we are tracking this child.
  bool child_present = ChildPresent();
  if (kReadOperation == operation_ && !child_present)
    return false;

  if (!child_present || !entry_->backend_->OpenEntry(key, &child_)) {
    if (!entry_->backend_->CreateEntry(key, &child_)) {
      child_ = NULL;
      result_ = net::ERR_CACHE_READ_FAILURE;
      return false;
    }
    // Write signature.
    InitChildData();
    return true;
  }

  // TODO(rvargas): Set/check a flag in EntryStore.

  scoped_refptr<net::WrappedIOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&child_data_));

  // Read signature.
  int rv = child_->ReadData(kSparseIndex, 0, buf, sizeof(child_data_), NULL);
  if (rv != sizeof(child_data_)) {
    result_ = net::ERR_CACHE_READ_FAILURE;
    return false;
  }

  // TODO(rvargas): Proper error handling and check magic etc.
  if (child_data_.header.signature != sparse_header_.signature) {
    result_ = net::ERR_CACHE_READ_FAILURE;
    return false;
  }

  return true;
}

void SparseControl::CloseChild() {
  scoped_refptr<net::WrappedIOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&child_data_));

  // Save the allocation bitmap before closing the child entry.
  int rv = child_->WriteData(kSparseIndex, 0, buf, sizeof(child_data_),
                             NULL, false);
  if (rv != sizeof(child_data_))
    DLOG(ERROR) << "Failed to save child data";
  child_->Close();
  child_ = NULL;
}

// If this entry is called entry_name, child entreies will be named something
// like Range_entry_name:XXX:YYY where XXX is the entry signature and YYY is the
// number of the particular child.
std::string SparseControl::GenerateChildKey() {
  return StringPrintf("Range_%s:%llx:%llx", entry_->GetKey().c_str(),
                      sparse_header_.signature, offset_ >> 20);
}

bool SparseControl::ChildPresent() {
  int child_bit = static_cast<int>(offset_ >> 20);
  if (children_map_.Size() < child_bit)
    return false;

  return children_map_.Get(child_bit);
}

void SparseControl::SetChildBit() {
  int child_bit = static_cast<int>(offset_ >> 20);

  // We may have to increase the bitmap of child entries.
  if (children_map_.Size() <= child_bit)
    children_map_.Resize(Bitmap::RequiredArraySize(child_bit + 1) * 32, true);

  children_map_.Set(child_bit, true);
}

void SparseControl::WriteSparseData() {
  scoped_refptr<net::IOBuffer> buf = new net::WrappedIOBuffer(
      reinterpret_cast<const char*>(children_map_.GetMap()));

  int len = children_map_.ArraySize() * 4;
  int rv = entry_->WriteData(kSparseIndex, sizeof(sparse_header_), buf, len,
                             NULL, false);
  if (rv != len) {
    DLOG(ERROR) << "Unable to save sparse map";
  }
}

bool SparseControl::VerifyRange() {
  DCHECK_GE(result_, 0);

  child_offset_ = static_cast<int>(offset_) & 0xfffff;
  child_len_ = std::min(buf_len_, 0x100000 - child_offset_);

  // We can write to anywhere in this child.
  if (operation_ != kReadOperation)
    return true;

  // Check that there are no holes in this range.
  int last_bit = (child_offset_ + child_len_ + 1023) >> 10;
  int start = child_offset_ >> 10;
  if (child_map_.FindNextBit(&start, last_bit, false)) {
    // Something is not here.
    if (start == child_offset_ >> 10)
      return false;

    // We have the first part.
    // TODO(rvargas): Avoid coming back here again after the actual read.
    child_len_ = (start << 10) - child_offset_;
  }
  return true;
}

void SparseControl::UpdateRange(int result) {
  if (result <= 0 || operation_ != kWriteOperation)
    return;

  // Write the bitmap.
  int last_bit = (child_offset_ + result + 1023) >> 10;
  child_map_.SetRange(child_offset_ >> 10, last_bit, true);

  // TODO(rvargas): Keep track of partial writes so that we don't consider the
  // whole block to be present.
}

void SparseControl::InitChildData() {
  memset(&child_data_, 0, sizeof(child_data_));
  child_data_.header = sparse_header_;

  scoped_refptr<net::WrappedIOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&child_data_));

  int rv = child_->WriteData(kSparseIndex, 0, buf, sizeof(child_data_),
                             NULL, false);
  if (rv != sizeof(child_data_))
    DLOG(ERROR) << "Failed to save child data";
  SetChildBit();
}

void SparseControl::DoChildrenIO() {
  while (DoChildIO()) continue;

  if (pending_ && finished_)
    DoUserCallback();
}

bool SparseControl::DoChildIO() {
  finished_ = true;
  if (!buf_len_ || result_ < 0)
    return false;

  if (!OpenChild())
    return false;

  if (!VerifyRange())
    return false;

  // We have more work to do. Let's not trigger a callback to the caller.
  finished_ = false;
  net::CompletionCallback* callback = user_callback_ ? &child_callback_ : NULL;

  int rv;
  if (kReadOperation == operation_) {
    rv = child_->ReadData(kSparseData, child_offset_, user_buf_, child_len_,
                          callback);
  } else {
    DCHECK(kWriteOperation == operation_);
    rv = child_->WriteData(kSparseData, child_offset_, user_buf_, child_len_,
                           callback, false);
  }

  if (rv == net::ERR_IO_PENDING) {
    if (!pending_) {
      pending_ = true;
      // The child will protect himself against closing the entry while IO is in
      // progress. However, this entry can still be closed, and that would not
      // be a good thing for us, so we increase the refcount until we're
      // finished doing sparse stuff.
      entry_->AddRef();
    }
    return false;
  }

  DoChildIOCompleted(rv);
  return true;
}

void SparseControl::DoChildIOCompleted(int result) {
  if (result < 0) {
    // We fail the whole operation if we encounter an error.
    result_ = result;
    return;
  }

  UpdateRange(result);

  result_ += result;
  offset_ += result;
  buf_len_ -= result;

  // We'll be reusing the user provided buffer for the next chunk.
  if (buf_len_)
    user_buf_->SetOffset(result_);
}

void SparseControl::OnChildIOCompleted(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DoChildIOCompleted(result);

  // We are running a callback from the message loop. It's time to restart what
  // we were doing before.
  DoChildrenIO();
}

void SparseControl::DoUserCallback() {
  DCHECK(user_callback_);
  net::CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  user_buf_ = NULL;
  pending_ = false;
  operation_ = kNoOperation;
  entry_->Release();  // Don't touch object after this line.
  c->Run(result_);
}

}  // namespace disk_cache
