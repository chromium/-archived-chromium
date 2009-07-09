// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SPARSE_CONTROL_H_
#define NET_DISK_CACHE_SPARSE_CONTROL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/disk_cache/bitmap.h"
#include "net/disk_cache/disk_format.h"

namespace net {
class IOBuffer;
class ReusedIOBuffer;
}

namespace disk_cache {

class Entry;
class EntryImpl;

// This class provides support for the sparse capabilities of the disk cache.
// Basically, sparse IO is directed from EntryImpl to this class, and we split
// the operation into multiple small pieces, sending each one to the
// appropriate entry. An instance of this class is asociated with each entry
// used directly for sparse operations (the entry passed in to the constructor).
class SparseControl {
 public:
  // The operation to perform.
  enum SparseOperation {
    kNoOperation,
    kReadOperation,
    kWriteOperation,
    kGetRangeOperation
  };

  explicit SparseControl(EntryImpl* entry)
      : entry_(entry), child_(NULL), operation_(kNoOperation), init_(false),
        child_map_(child_data_.bitmap, kNumSparseBits, kNumSparseBits / 32),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            child_callback_(this, &SparseControl::OnChildIOCompleted)),
        user_callback_(NULL) {}
  ~SparseControl();

  // Initializes the object for the current entry. If this entry already stores
  // sparse data, or can be used to do it, it updates the relevant information
  // on disk and returns net::OK. Otherwise it returns a net error code.
  int Init();

  // Performs an actual sparse read or write operation for this entry. |op| is
  // the operation to perform, |offset| is the desired sparse offset, |buf| and
  // |buf_len| specify the actual data to use and |callback| is the callback
  // to use for asynchronous operations. See the description of the Read /
  // WriteSparseData for details about the arguments. The return value is the
  // number of bytes read or written, or a net error code.
  int StartIO(SparseOperation op, int64 offset, net::IOBuffer* buf,
              int buf_len, net::CompletionCallback* callback);

  // Implements Entry::GetAvailableRange().
  int GetAvailableRange(int64 offset, int len, int64* start);

  // Deletes the children entries of |entry|.
  static void DeleteChildren(EntryImpl* entry);

 private:
  // Creates a new sparse entry or opens an aready created entry from disk.
  // These methods just read / write the required info from disk for the current
  // entry, and verify that everything is correct. The return value is a net
  // error code.
  int CreateSparseEntry();
  int OpenSparseEntry(int data_len);

  // Opens and closes a child entry. A child entry is a regular EntryImpl object
  // with a key derived from the key of the resource to store and the range
  // stored by that child.
  bool OpenChild();
  void CloseChild();
  std::string GenerateChildKey();

  // Returns true if the required child is tracked by the parent entry, i.e. it
  // was already created.
  bool ChildPresent();

  // Starts tracking this child. A new child entry was created so we must set
  // the corresponding bit on the bitmap of children.
  void SetChildBit();

  // Writes to disk the tracking information for this entry.
  void WriteSparseData();

  // Verify that the range to be accessed for the current child is appropriate.
  // Returns false if an error is detected or there is no need to perform the
  // current IO operation (for instance if the required range is not stored by
  // the child).
  bool VerifyRange();

  // Updates the contents bitmap for the current range, based on the result of
  // the current operation.
  void UpdateRange(int result);

  // Initializes the sparse info for the current child.
  void InitChildData();

  // Iterates through all the children needed to complete the current operation.
  void DoChildrenIO();

  // Performs a single operation with the current child. Returns true when we
  // should move on to the next child and false when we should interrupt our
  // work.
  bool DoChildIO();

  // Performs the required work for GetAvailableRange for one child.
  int DoGetAvailableRange();

  // Performs the required work after a single IO operations finishes.
  void DoChildIOCompleted(int result);

  // Invoked by the callback of asynchronous operations.
  void OnChildIOCompleted(int result);

  // Reports to the user that we are done.
  void DoUserCallback();

  EntryImpl* entry_;  // The sparse entry.
  Entry* child_;  // The current child entry.
  SparseOperation operation_;
  bool pending_;  // True if any child IO operation returned pending.
  bool finished_;
  bool init_;
  bool range_found_;  // True if GetAvailableRange found something.

  SparseHeader sparse_header_;  // Data about the children of entry_.
  Bitmap children_map_;  // The actual bitmap of children.
  SparseData child_data_;  // Parent and allocation map of child_.
  Bitmap child_map_;  // The allocation map as a bitmap.

  net::CompletionCallbackImpl<SparseControl> child_callback_;
  net::CompletionCallback* user_callback_;
  int64 offset_;  // Current sparse offset.
  scoped_refptr<net::ReusedIOBuffer> user_buf_;
  int buf_len_;  // Bytes to read or write.
  int child_offset_;  // Offset to use for the current child.
  int child_len_;  // Bytes to read or write for this child.
  int result_;

  DISALLOW_COPY_AND_ASSIGN(SparseControl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SPARSE_CONTROL_H_
