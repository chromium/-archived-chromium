// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_STORAGE_BLOCK_H__
#define NET_DISK_CACHE_STORAGE_BLOCK_H__

#include "net/disk_cache/addr.h"
#include "net/disk_cache/mapped_file.h"

namespace disk_cache {

class EntryImpl;

// This class encapsulates common behavior of a single "block" of data that is
// stored on a block-file. It implements the FileBlock interface, so it can be
// serialized directly to the backing file.
// This object provides a memory buffer for the related data, and it can be used
// to actually share that memory with another instance of the class.
//
// The following example shows how to share storage with another object:
//    StorageBlock<TypeA> a(file, address);
//    StorageBlock<TypeB> b(file, address);
//    a.Load();
//    DoSomething(a.Data());
//    b.SetData(a.Data());
//    ModifySomething(b.Data());
//    // Data modified on the previous call will be saved by b's destructor.
//    b.set_modified();
template<typename T>
class StorageBlock : public FileBlock {
 public:
  StorageBlock(MappedFile* file, Addr address);
  virtual ~StorageBlock();

  // FileBlock interface.
  virtual void* buffer() const;
  virtual size_t size() const;
  virtual int offset() const;

  // Allows the overide of dummy values passed on the constructor.
  bool LazyInit(MappedFile* file, Addr address);

  // Sets the internal storage to share the momory provided by other instance.
  void SetData(T* other);

  // Sets the object to lazily save the in-memory data on destruction.
  void set_modified();

  // Gets a pointer to the internal storage (allocates storage if needed).
  T* Data();

  // Returns true if there is data associated with this object.
  bool HasData() const;

  const Addr address() const;

  // Loads and store the data.
  bool Load();
  bool Store();

 private:
  void AllocateData();
  void DeleteData();

  T* data_;
  MappedFile* file_;
  Addr address_;
  bool modified_;
  bool own_data_;  // Is data_ owned by this object or shared with someone else.
  bool extended_;  // Used to store an entry of more than one block.

  DISALLOW_EVIL_CONSTRUCTORS(StorageBlock);
};

typedef StorageBlock<EntryStore> CacheEntryBlock;
typedef StorageBlock<RankingsNode> CacheRankingsBlock;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_STORAGE_BLOCK_H__
