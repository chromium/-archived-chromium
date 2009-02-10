// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/entry_impl.h"

#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/cache_util.h"

using base::Time;
using base::TimeDelta;

namespace {

// Index for the file used to store the key, if any (files_[kKeyFileIndex]).
const int kKeyFileIndex = 3;

// This class implements FileIOCallback to buffer the callback from a file IO
// operation from the actual net class.
class SyncCallback: public disk_cache::FileIOCallback {
 public:
  SyncCallback(disk_cache::EntryImpl* entry,
               net::CompletionCallback* callback )
      : entry_(entry), callback_(callback) {
    entry->AddRef();
    entry->IncrementIoCount();
  }
  ~SyncCallback() {}

  virtual void OnFileIOComplete(int bytes_copied);
  void Discard();
 private:
  disk_cache::EntryImpl* entry_;
  net::CompletionCallback* callback_;

  DISALLOW_EVIL_CONSTRUCTORS(SyncCallback);
};

void SyncCallback::OnFileIOComplete(int bytes_copied) {
  entry_->DecrementIoCount();
  entry_->Release();
  if (callback_)
    callback_->Run(bytes_copied);
  delete this;
}

void SyncCallback::Discard() {
  callback_ = NULL;
  OnFileIOComplete(0);
}

// Clears buffer before offset and after valid_len, knowing that the size of
// buffer is kMaxBlockSize.
void ClearInvalidData(char* buffer, int offset, int valid_len) {
  DCHECK(offset >= 0);
  DCHECK(valid_len >= 0);
  DCHECK(disk_cache::kMaxBlockSize >= offset + valid_len);
  if (offset)
    memset(buffer, 0, offset);
  int end = disk_cache::kMaxBlockSize - offset - valid_len;
  if (end)
    memset(buffer + offset + valid_len, 0, end);
}

}  // namespace

namespace disk_cache {

EntryImpl::EntryImpl(BackendImpl* backend, Addr address)
    : entry_(NULL, Addr(0)), node_(NULL, Addr(0)) {
  entry_.LazyInit(backend->File(address), address);
  doomed_ = false;
  backend_ = backend;
  for (int i = 0; i < NUM_STREAMS; i++)
    unreported_size_[i] = 0;
}

// When an entry is deleted from the cache, we clean up all the data associated
// with it for two reasons: to simplify the reuse of the block (we know that any
// unused block is filled with zeros), and to simplify the handling of write /
// read partial information from an entry (don't have to worry about returning
// data related to a previous cache entry because the range was not fully
// written before).
EntryImpl::~EntryImpl() {
  if (doomed_) {
    UMA_HISTOGRAM_COUNTS(L"DiskCache.DeleteHeader", GetDataSize(0));
    UMA_HISTOGRAM_COUNTS(L"DiskCache.DeleteData", GetDataSize(1));
    for (int index = 0; index < NUM_STREAMS; index++) {
      Addr address(entry_.Data()->data_addr[index]);
      if (address.is_initialized()) {
        DeleteData(address, index);
        backend_->ModifyStorageSize(entry_.Data()->data_size[index] -
                                        unreported_size_[index], 0);
      }
    }
    Addr address(entry_.Data()->long_key);
    DeleteData(address, kKeyFileIndex);
    backend_->ModifyStorageSize(entry_.Data()->key_len, 0);

    memset(node_.buffer(), 0, node_.size());
    memset(entry_.buffer(), 0, entry_.size());
    node_.Store();
    entry_.Store();

    backend_->DeleteBlock(node_.address(), false);
    backend_->DeleteBlock(entry_.address(), false);
  } else {
    bool ret = true;
    for (int index = 0; index < NUM_STREAMS; index++) {
      if (user_buffers_[index].get()) {
        if (!(ret = Flush(index, entry_.Data()->data_size[index], false)))
          LOG(ERROR) << "Failed to save user data";
      } else if (unreported_size_[index]) {
        backend_->ModifyStorageSize(
            entry_.Data()->data_size[index] - unreported_size_[index],
            entry_.Data()->data_size[index]);
      }
    }
    if (node_.HasData() && this == node_.Data()->pointer) {
      // We have to do this after Flush because we may trigger a cache trim from
      // there, and technically this entry should be "in use".
      node_.Data()->pointer = NULL;
      node_.set_modified();
    }

    if (!ret) {
      // There was a failure writing the actual data. Mark the entry as dirty.
      int current_id = backend_->GetCurrentEntryId();
      node_.Data()->dirty = current_id == 1 ? -1 : current_id - 1;
      node_.Store();
    } else if (node_.HasData() && node_.Data()->dirty) {
      node_.Data()->dirty = 0;
      node_.Store();
    }
  }

  backend_->CacheEntryDestroyed();
}

void EntryImpl::Doom() {
  if (doomed_)
    return;

  SetPointerForInvalidEntry(backend_->GetCurrentEntryId());
  backend_->InternalDoomEntry(this);
}

void EntryImpl::Close() {
  Release();
}

std::string EntryImpl::GetKey() const {
  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  if (entry->Data()->key_len > kMaxInternalKeyLength) {
    Addr address(entry->Data()->long_key);
    DCHECK(address.is_initialized());
    COMPILE_ASSERT(NUM_STREAMS == kKeyFileIndex, invalid_key_index);
    File* file = const_cast<EntryImpl*>(this)->GetBackingFile(address,
                                                              kKeyFileIndex);

    size_t offset = 0;
    if (address.is_block_file())
      offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

    std::string key;
    if (!file || !file->Read(WriteInto(&key, entry->Data()->key_len + 1),
                             entry->Data()->key_len + 1, offset))
      key.clear();
    return key;
  } else {
    return std::string(entry->Data()->key);
  }
}

Time EntryImpl::GetLastUsed() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_used);
}

Time EntryImpl::GetLastModified() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_modified);
}

int32 EntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= NUM_STREAMS)
    return 0;

  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  return entry->Data()->data_size[index];
}

int EntryImpl::ReadData(int index, int offset, char* buf, int buf_len,
                        net::CompletionCallback* completion_callback) {
  DCHECK(node_.Data()->dirty);
  if (index < 0 || index >= NUM_STREAMS)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = entry_.Data()->data_size[index];
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  Time start = Time::Now();
  static Histogram stats(L"DiskCache.ReadTime", TimeDelta::FromMilliseconds(1),
                         TimeDelta::FromSeconds(10), 50);
  stats.SetFlags(kUmaTargetedHistogramFlag);

  if (offset + buf_len > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  backend_->OnEvent(Stats::READ_DATA);

  if (user_buffers_[index].get()) {
    // Complete the operation locally.
    DCHECK(kMaxBlockSize >= offset + buf_len);
    memcpy(buf , user_buffers_[index].get() + offset, buf_len);
    stats.AddTime(Time::Now() - start);
    return buf_len;
  }

  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(address.is_initialized());
  if (!address.is_initialized())
    return net::ERR_FAILED;

  File* file = GetBackingFile(address, index);
  if (!file)
    return net::ERR_FAILED;

  size_t file_offset = offset;
  if (address.is_block_file())
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;

  SyncCallback* io_callback = NULL;
  if (completion_callback)
    io_callback = new SyncCallback(this, completion_callback);

  bool completed;
  if (!file->Read(buf, buf_len, file_offset, io_callback, &completed)) {
    if (io_callback)
      io_callback->Discard();
    return net::ERR_FAILED;
  }

  if (io_callback && completed)
    io_callback->Discard();

  stats.AddTime(Time::Now() - start);
  return (completed || !completion_callback) ? buf_len : net::ERR_IO_PENDING;
}

int EntryImpl::WriteData(int index, int offset, const char* buf, int buf_len,
                         net::CompletionCallback* completion_callback,
                         bool truncate) {
  DCHECK(node_.Data()->dirty);
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
    backend_->TooMuchStorageRequested(size);
    return net::ERR_FAILED;
  }

  Time start = Time::Now();
  static Histogram stats(L"DiskCache.WriteTime", TimeDelta::FromMilliseconds(1),
                         TimeDelta::FromSeconds(10), 50);
  stats.SetFlags(kUmaTargetedHistogramFlag);

  // Read the size at this point (it may change inside prepare).
  int entry_size = entry_.Data()->data_size[index];
  if (!PrepareTarget(index, offset, buf_len, truncate))
    return net::ERR_FAILED;

  if (entry_size < offset + buf_len) {
    unreported_size_[index] += offset + buf_len - entry_size;
    entry_.Data()->data_size[index] = offset + buf_len;
    entry_.set_modified();
    if (!buf_len)
      truncate = true;  // Force file extension.
  } else if (truncate) {
      // If the size was modified inside PrepareTarget, we should not do
      // anything here.
      if ((entry_size > offset + buf_len) &&
          (entry_size == entry_.Data()->data_size[index])) {
        unreported_size_[index] += offset + buf_len - entry_size;
        entry_.Data()->data_size[index] = offset + buf_len;
        entry_.set_modified();
      } else {
        // Nothing to truncate.
        truncate = false;
      }
  }

  UpdateRank(true);

  backend_->OnEvent(Stats::WRITE_DATA);

  if (user_buffers_[index].get()) {
    // Complete the operation locally.
    DCHECK(kMaxBlockSize >= offset + buf_len);
    memcpy(user_buffers_[index].get() + offset, buf, buf_len);
    stats.AddTime(Time::Now() - start);
    return buf_len;
  }

  Addr address(entry_.Data()->data_addr[index]);
  File* file = GetBackingFile(address, index);
  if (!file)
    return net::ERR_FAILED;

  size_t file_offset = offset;
  if (address.is_block_file()) {
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;
  } else if (truncate) {
    if (!file->SetLength(offset + buf_len))
      return net::ERR_FAILED;
  }

  if (!buf_len)
    return 0;

  SyncCallback* io_callback = NULL;
  if (completion_callback)
    io_callback = new SyncCallback(this, completion_callback);

  bool completed;
  if (!file->Write(buf, buf_len, file_offset, io_callback, &completed)) {
    if (io_callback)
      io_callback->Discard();
    return net::ERR_FAILED;
  }

  if (io_callback && completed)
    io_callback->Discard();

  stats.AddTime(Time::Now() - start);
  return (completed || !completion_callback) ? buf_len : net::ERR_IO_PENDING;
}

uint32 EntryImpl::GetHash() {
  return entry_.Data()->hash;
}

bool EntryImpl::CreateEntry(Addr node_address, const std::string& key,
                            uint32 hash) {
  Trace("Create entry In");
  EntryStore* entry_store = entry_.Data();
  RankingsNode* node = node_.Data();
  memset(entry_store, 0, sizeof(EntryStore) * entry_.address().num_blocks());
  memset(node, 0, sizeof(RankingsNode));
  if (!node_.LazyInit(backend_->File(node_address), node_address))
    return false;

  entry_store->rankings_node = node_address.value();
  node->contents = entry_.address().value();
  node->pointer = this;

  entry_store->hash = hash;
  entry_store->creation_time = Time::Now().ToInternalValue();
  entry_store->key_len = static_cast<int32>(key.size());
  if (entry_store->key_len > kMaxInternalKeyLength) {
    Addr address(0);
    if (!CreateBlock(entry_store->key_len + 1, &address))
      return false;

    entry_store->long_key = address.value();
    File* file = GetBackingFile(address, kKeyFileIndex);

    size_t offset = 0;
    if (address.is_block_file())
      offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

    if (!file || !file->Write(key.data(), key.size(), offset)) {
      DeleteData(address, kKeyFileIndex);
      return false;
    }

    if (address.is_separate_file())
      file->SetLength(key.size() + 1);
  } else {
    memcpy(entry_store->key, key.data(), key.size());
    entry_store->key[key.size()] = '\0';
  }
  backend_->ModifyStorageSize(0, static_cast<int32>(key.size()));
  node->dirty = backend_->GetCurrentEntryId();
  Log("Create Entry ");
  return true;
}

bool EntryImpl::IsSameEntry(const std::string& key, uint32 hash) {
  if (entry_.Data()->hash != hash ||
      static_cast<size_t>(entry_.Data()->key_len) != key.size())
    return false;

  std::string my_key = GetKey();
  return key.compare(my_key) ? false : true;
}

void EntryImpl::InternalDoom() {
  DCHECK(node_.HasData());
  if (!node_.Data()->dirty) {
    node_.Data()->dirty = backend_->GetCurrentEntryId();
    node_.Store();
  }
  doomed_ = true;
}

CacheAddr EntryImpl::GetNextAddress() {
  return entry_.Data()->next;
}

void EntryImpl::SetNextAddress(Addr address) {
  entry_.Data()->next = address.value();
  bool success = entry_.Store();
  DCHECK(success);
}

bool EntryImpl::LoadNodeAddress() {
  Addr address(entry_.Data()->rankings_node);
  if (!node_.LazyInit(backend_->File(address), address))
    return false;
  return node_.Load();
}

EntryImpl* EntryImpl::Update(EntryImpl* entry) {
  DCHECK(entry->rankings()->HasData());

  RankingsNode* rankings = entry->rankings()->Data();
  if (rankings->pointer) {
    // Already in memory. Prevent clearing the dirty flag on the destructor.
    rankings->dirty = 0;
    EntryImpl* real_node = reinterpret_cast<EntryImpl*>(rankings->pointer);
    real_node->AddRef();
    entry->Release();
    return real_node;
  } else {
    rankings->dirty = entry->backend_->GetCurrentEntryId();
    rankings->pointer = entry;
    if (!entry->rankings()->Store()) {
      entry->Release();
      return NULL;
    }
    return entry;
  }
}

bool EntryImpl::IsDirty(int32 current_id) {
  DCHECK(node_.HasData());
  return node_.Data()->dirty && current_id != node_.Data()->dirty;
}

void EntryImpl::ClearDirtyFlag() {
  node_.Data()->dirty = 0;
}

void EntryImpl::SetPointerForInvalidEntry(int32 new_id) {
  node_.Data()->dirty = new_id;
  node_.Data()->pointer = this;
  node_.Store();
}

bool EntryImpl::SanityCheck() {
  if (!entry_.Data()->rankings_node || !entry_.Data()->key_len)
    return false;

  Addr rankings_addr(entry_.Data()->rankings_node);
  if (!rankings_addr.is_initialized() || rankings_addr.is_separate_file() ||
      rankings_addr.file_type() != RANKINGS)
    return false;

  Addr next_addr(entry_.Data()->next);
  if (next_addr.is_initialized() &&
      (next_addr.is_separate_file() || next_addr.file_type() != BLOCK_256))
    return false;

  return true;
}

void EntryImpl::IncrementIoCount() {
  backend_->IncrementIoCount();
}

void EntryImpl::DecrementIoCount() {
  backend_->DecrementIoCount();
}

void EntryImpl::SetTimes(base::Time last_used, base::Time last_modified) {
  node_.Data()->last_used = last_used.ToInternalValue();
  node_.Data()->last_modified = last_modified.ToInternalValue();
  node_.set_modified();
}

bool EntryImpl::CreateDataBlock(int index, int size) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(index >= 0 && index < NUM_STREAMS);

  if (!CreateBlock(size, &address))
    return false;

  entry_.Data()->data_addr[index] = address.value();
  entry_.Store();
  return true;
}

bool EntryImpl::CreateBlock(int size, Addr* address) {
  DCHECK(!address->is_initialized());

  FileType file_type = Addr::RequiredFileType(size);
  if (EXTERNAL == file_type) {
    if (size > backend_->MaxFileSize())
      return false;
    if (!backend_->CreateExternalFile(address))
      return false;
  } else {
    int num_blocks = (size + Addr::BlockSizeForFileType(file_type) - 1) /
                     Addr::BlockSizeForFileType(file_type);

    if (!backend_->CreateBlock(file_type, num_blocks, address))
      return false;
  }
  return true;
}

void EntryImpl::DeleteData(Addr address, int index) {
  if (!address.is_initialized())
    return;
  if (address.is_separate_file()) {
    if (files_[index])
      files_[index] = NULL;  // Releases the object.

    if (!DeleteCacheFile(backend_->GetFileName(address))) {
      UMA_HISTOGRAM_COUNTS(L"DiskCache.DeleteFailed", 1);
      LOG(ERROR) << "Failed to delete " << backend_->GetFileName(address) <<
                    " from the cache.";
    }
  } else {
    backend_->DeleteBlock(address, true);
  }
}

void EntryImpl::UpdateRank(bool modified) {
  if (!doomed_) {
    // Everything is handled by the backend.
    backend_->UpdateRank(this, true);
    return;
  }

  Time current = Time::Now();
  node_.Data()->last_used = current.ToInternalValue();

  if (modified)
    node_.Data()->last_modified = current.ToInternalValue();
}

File* EntryImpl::GetBackingFile(Addr address, int index) {
  File* file;
  if (address.is_separate_file())
    file = GetExternalFile(address, index);
  else
    file = backend_->File(address);
  return file;
}

File* EntryImpl::GetExternalFile(Addr address, int index) {
  DCHECK(index >= 0 && index <= kKeyFileIndex);
  if (!files_[index].get()) {
    // For a key file, use mixed mode IO.
    scoped_refptr<File> file(new File(kKeyFileIndex == index));
    if (file->Init(backend_->GetFileName(address)))
      files_[index].swap(file);
  }
  return files_[index].get();
}

bool EntryImpl::PrepareTarget(int index, int offset, int buf_len,
                              bool truncate) {
  Addr address(entry_.Data()->data_addr[index]);
  if (address.is_initialized() || user_buffers_[index].get())
    return GrowUserBuffer(index, offset, buf_len, truncate);

  if (offset + buf_len > kMaxBlockSize)
    return CreateDataBlock(index, offset + buf_len);

  user_buffers_[index].reset(new char[kMaxBlockSize]);

  // Overwrite the parts of the buffer that are not going to be written
  // by the current operation (and yes, let's assume that nothing is going
  // to fail, and we'll actually write over the part that we are not cleaning
  // here). The point is to avoid writing random stuff to disk later on.
  ClearInvalidData(user_buffers_[index].get(), offset, buf_len);

  return true;
}

// We get to this function with some data already stored. If there is a
// truncation that results on data stored internally, we'll explicitly
// handle the case here.
bool EntryImpl::GrowUserBuffer(int index, int offset, int buf_len,
                               bool truncate) {
  Addr address(entry_.Data()->data_addr[index]);

  if (offset + buf_len > kMaxBlockSize) {
    // The data has to be stored externally.
    if (address.is_initialized()) {
      if (address.is_separate_file())
        return true;
      if (!MoveToLocalBuffer(index))
        return false;
    }
    return Flush(index, offset + buf_len, true);
  }

  if (!address.is_initialized()) {
    DCHECK(user_buffers_[index].get());
    if (truncate)
      ClearInvalidData(user_buffers_[index].get(), 0, offset + buf_len);
    return true;
  }
  if (address.is_separate_file()) {
    if (!truncate)
      return true;
    return ImportSeparateFile(index, offset, buf_len);
  }

  // At this point we are dealing with data stored on disk, inside a block file.
  if (offset + buf_len <= address.BlockSize() * address.num_blocks())
    return true;

  // ... and the allocated block has to change.
  if (!MoveToLocalBuffer(index))
    return false;

  int clear_start = entry_.Data()->data_size[index];
  if (truncate)
    clear_start = std::min(clear_start, offset + buf_len);
  else if (offset < clear_start)
    clear_start = std::max(offset + buf_len, clear_start);

  // Clear the end of the buffer.
  ClearInvalidData(user_buffers_[index].get(), 0, clear_start);
  return true;
}

bool EntryImpl::MoveToLocalBuffer(int index) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(!user_buffers_[index].get());
  DCHECK(address.is_initialized());
  scoped_array<char> buffer(new char[kMaxBlockSize]);

  File* file = GetBackingFile(address, index);
  size_t len = entry_.Data()->data_size[index];
  size_t offset = 0;

  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  if (!file || !file->Read(buffer.get(), len, offset, NULL, NULL))
    return false;

  DeleteData(address, index);
  entry_.Data()->data_addr[index] = 0;
  entry_.Store();

  // If we lose this entry we'll see it as zero sized.
  backend_->ModifyStorageSize(static_cast<int>(len) - unreported_size_[index],
                              0);
  unreported_size_[index] = static_cast<int>(len);

  user_buffers_[index].swap(buffer);
  return true;
}

bool EntryImpl::ImportSeparateFile(int index, int offset, int buf_len) {
  if (entry_.Data()->data_size[index] > offset + buf_len) {
    unreported_size_[index] += offset + buf_len -
                               entry_.Data()->data_size[index];
    entry_.Data()->data_size[index] = offset + buf_len;
  }

  if (!MoveToLocalBuffer(index))
    return false;

  // Clear the end of the buffer.
  ClearInvalidData(user_buffers_[index].get(), 0, offset + buf_len);
  return true;
}

// The common scenario is that this is called from the destructor of the entry,
// to write to disk what we have buffered. We don't want to hold the destructor
// until the actual IO finishes, so we'll send an asynchronous write that will
// free up the memory containing the data. To be consistent, this method always
// returns with the buffer freed up (on success).
bool EntryImpl::Flush(int index, int size, bool async) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(user_buffers_[index].get());
  DCHECK(!address.is_initialized());

  if (!size)
    return true;

  if (!CreateDataBlock(index, size))
    return false;

  address.set_value(entry_.Data()->data_addr[index]);

  File* file = GetBackingFile(address, index);
  size_t len = entry_.Data()->data_size[index];
  size_t offset = 0;
  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  // We just told the backend to store len bytes for real.
  DCHECK(len == static_cast<size_t>(unreported_size_[index]));
  backend_->ModifyStorageSize(0, static_cast<int>(len));
  unreported_size_[index] = 0;

  if (!file)
    return false;

  // TODO(rvargas): figure out if it's worth to re-enable posting operations.
  // Right now it is only used from GrowUserBuffer, not the destructor, and
  // it is not accounted for from the point of view of the total number of
  // pending operations of the cache. It is also racing with the actual write
  // on the GrowUserBuffer path because there is no code to exclude the range
  // that is going to be written.
  async = false;
  if (async) {
    if (!file->PostWrite(user_buffers_[index].get(), len, offset))
      return false;
  } else {
    if (!file->Write(user_buffers_[index].get(), len, offset, NULL, NULL))
      return false;
    user_buffers_[index].reset(NULL);
  }

  // The buffer is deleted from the PostWrite operation.
  user_buffers_[index].release();

  return true;
}

void EntryImpl::Log(const char* msg) {
  void* pointer = NULL;
  int dirty = 0;
  if (node_.HasData()) {
    pointer = node_.Data()->pointer;
    dirty = node_.Data()->dirty;
  }

  Trace("%s 0x%p 0x%x 0x%x", msg, reinterpret_cast<void*>(this),
        entry_.address().value(), node_.address().value());

  Trace("  data: 0x%x 0x%x 0x%x", entry_.Data()->data_addr[0],
        entry_.Data()->data_addr[1], entry_.Data()->long_key);

  Trace("  doomed: %d 0x%p 0x%x", doomed_, pointer, dirty);
}

}  // namespace disk_cache

