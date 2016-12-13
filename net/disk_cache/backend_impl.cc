// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/backend_impl.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/timer.h"
#include "base/worker_pool.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/errors.h"
#include "net/disk_cache/hash.h"
#include "net/disk_cache/file.h"

// Uncomment this to use the new eviction algorithm.
// #define USE_NEW_EVICTION

// This has to be defined before including histogram_macros.h from this file.
#define NET_DISK_CACHE_BACKEND_IMPL_CC_
#include "net/disk_cache/histogram_macros.h"

using base::Time;
using base::TimeDelta;

namespace {

const wchar_t* kIndexName = L"index";
const int kMaxOldFolders = 100;

// Seems like ~240 MB correspond to less than 50k entries for 99% of the people.
const int k64kEntriesStore = 240 * 1000 * 1000;
const int kBaseTableLen = 64 * 1024;
const int kDefaultCacheSize = 80 * 1024 * 1024;

int DesiredIndexTableLen(int32 storage_size) {
  if (storage_size <= k64kEntriesStore)
    return kBaseTableLen;
  if (storage_size <= k64kEntriesStore * 2)
    return kBaseTableLen * 2;
  if (storage_size <= k64kEntriesStore * 4)
    return kBaseTableLen * 4;
  if (storage_size <= k64kEntriesStore * 8)
    return kBaseTableLen * 8;

  // The biggest storage_size for int32 requires a 4 MB table.
  return kBaseTableLen * 16;
}

int MaxStorageSizeForTable(int table_len) {
  return table_len * (k64kEntriesStore / kBaseTableLen);
}

size_t GetIndexSize(int table_len) {
  size_t table_size = sizeof(disk_cache::CacheAddr) * table_len;
  return sizeof(disk_cache::IndexHeader) + table_size;
}

// ------------------------------------------------------------------------

// Returns a fully qualified name from path and name, using a given name prefix
// and index number. For instance, if the arguments are "/foo", "bar" and 5, it
// will return "/foo/old_bar_005".
std::wstring GetPrefixedName(const std::wstring& path, const std::wstring& name,
                             int index) {
  std::wstring prefixed(path);
  std::wstring tmp = StringPrintf(L"%ls%ls_%03d", L"old_", name.c_str(), index);
  file_util::AppendToPath(&prefixed, tmp);
  return prefixed;
}

// This is a simple Task to cleanup old caches.
class CleanupTask : public Task {
 public:
  CleanupTask(const std::wstring& path, const std::wstring& name)
      : path_(path), name_(name) {}

  virtual void Run();

 private:
  std::wstring path_;
  std::wstring name_;
  DISALLOW_EVIL_CONSTRUCTORS(CleanupTask);
};

void CleanupTask::Run() {
  for (int i = 0; i < kMaxOldFolders; i++) {
    std::wstring to_delete = GetPrefixedName(path_, name_, i);
    disk_cache::DeleteCache(to_delete, true);
  }
}

// Returns a full path to rename the current cache, in order to delete it. path
// is the current folder location, and name is the current folder name.
std::wstring GetTempCacheName(const std::wstring& path,
                              const std::wstring& name) {
  // We'll attempt to have up to kMaxOldFolders folders for deletion.
  for (int i = 0; i < kMaxOldFolders; i++) {
    std::wstring to_delete = GetPrefixedName(path, name, i);
    if (!file_util::PathExists(to_delete))
      return to_delete;
  }
  return std::wstring();
}

// Moves the cache files to a new folder and creates a task to delete them.
bool DelayedCacheCleanup(const std::wstring& full_path) {
  FilePath current_path = FilePath::FromWStringHack(full_path);
  current_path = current_path.StripTrailingSeparators();

  std::wstring path = current_path.DirName().ToWStringHack();
  std::wstring name = current_path.BaseName().ToWStringHack();

  std::wstring to_delete = GetTempCacheName(path, name);
  if (to_delete.empty()) {
    LOG(ERROR) << "Unable to get another cache folder";
    return false;
  }

  if (!disk_cache::MoveCache(full_path.c_str(), to_delete.c_str())) {
    LOG(ERROR) << "Unable to rename cache folder";
    return false;
  }

#if defined(OS_WIN)
  WorkerPool::PostTask(FROM_HERE, new CleanupTask(path, name), true);
#elif defined(OS_POSIX)
  // TODO(rvargas): Use the worker pool.
  MessageLoop::current()->PostTask(FROM_HERE, new CleanupTask(path, name));
#endif
  return true;
}

// Sets |current_group| for the current experiment. Returns false if the files
// should be discarded.
bool InitExperiment(int* current_group) {
  if (*current_group == 3 || *current_group == 4) {
    // Discard current cache for groups 3 and 4.
    return false;
  }

  if (*current_group <= 5) {
    // Re-load the two groups.
    int option = base::RandInt(0, 9);

    if (option > 1) {
      // 80% will be out of the experiment.
      *current_group = 9;
    } else {
      *current_group = option + 6;
    }
  }

  // The current groups should be:
  // 6 control. (~10%)
  // 7 new eviction, upgraded data. (~10%)
  // 8 new eviction, from new files.
  // 9 out. (~80%)

  UMA_HISTOGRAM_CACHE_ERROR("DiskCache.Experiment", *current_group);

  // Current experiment already set.
  return true;
}

}  // namespace

// ------------------------------------------------------------------------

namespace disk_cache {

// If the initialization of the cache fails, and force is true, we will discard
// the whole cache and create a new one. In order to process a potentially large
// number of files, we'll rename the cache folder to old_ + original_name +
// number, (located on the same parent folder), and spawn a worker thread to
// delete all the files on all the stale cache folders. The whole process can
// still fail if we are not able to rename the cache folder (for instance due to
// a sharing violation), and in that case a cache for this profile (on the
// desired path) cannot be created.
Backend* CreateCacheBackend(const std::wstring& full_path, bool force,
                            int max_bytes, net::CacheType type) {
  BackendImpl* cache = new BackendImpl(full_path);
  cache->SetMaxSize(max_bytes);
  cache->SetType(type);
  if (cache->Init())
    return cache;

  delete cache;
  if (!force)
    return NULL;

  if (!DelayedCacheCleanup(full_path))
    return NULL;

  // The worker thread will start deleting files soon, but the original folder
  // is not there anymore... let's create a new set of files.
  cache = new BackendImpl(full_path);
  cache->SetMaxSize(max_bytes);
  cache->SetType(type);
  if (cache->Init())
    return cache;

  delete cache;
  LOG(ERROR) << "Unable to create cache";
  return NULL;
}

int PreferedCacheSize(int64 available) {
  // If there is not enough space to use kDefaultCacheSize, use 80% of the
  // available space.
  if (available < kDefaultCacheSize)
    return static_cast<int32>(available * 8 / 10);

  // Don't use more than 10% of the available space.
  if (available < 10 * kDefaultCacheSize)
    return kDefaultCacheSize;

  // Use 10% of the free space until we reach 2.5 * kDefaultCacheSize.
  if (available < static_cast<int64>(kDefaultCacheSize) * 25)
    return static_cast<int32>(available / 10);

  // After reaching our target size (2.5 * kDefaultCacheSize), attempt to use
  // 1% of the availabe space.
  if (available < static_cast<int64>(kDefaultCacheSize) * 100)
    return kDefaultCacheSize * 5 / 2;

  int64 one_percent = available / 100;
  if (one_percent > kint32max)
    return kint32max;

  return static_cast<int32>(one_percent);
}

// ------------------------------------------------------------------------

bool BackendImpl::Init() {
  DCHECK(!init_);
  if (init_)
    return false;

#ifdef USE_NEW_EVICTION
  new_eviction_ = true;
  user_flags_ |= kNewEviction;
#endif

  bool create_files = false;
  if (!InitBackingStore(&create_files)) {
    ReportError(ERR_STORAGE_ERROR);
    return false;
  }

  num_refs_ = num_pending_io_ = max_refs_ = 0;

  if (!restarted_) {
    trace_object_ = TraceObject::GetTraceObject();
    // Create a recurrent timer of 30 secs.
    int timer_delay = unit_test_ ? 1000 : 30000;
    timer_.Start(TimeDelta::FromMilliseconds(timer_delay), this,
                 &BackendImpl::OnStatsTimer);
  }

  init_ = true;
  if (!InitExperiment(&data_->header.experiment))
    return false;

  if (data_->header.experiment > 6 && data_->header.experiment < 9)
    new_eviction_ = true;

  if (!CheckIndex()) {
    ReportError(ERR_INIT_FAILED);
    return false;
  }

  // We don't care if the value overflows. The only thing we care about is that
  // the id cannot be zero, because that value is used as "not dirty".
  // Increasing the value once per second gives us many years before a we start
  // having collisions.
  data_->header.this_id++;
  if (!data_->header.this_id)
    data_->header.this_id++;

  if (data_->header.crash) {
    ReportError(ERR_PREVIOUS_CRASH);
  } else {
    ReportError(0);
    data_->header.crash = 1;
  }

  if (!block_files_.Init(create_files))
    return false;

  // stats_ and rankings_ may end up calling back to us so we better be enabled.
  disabled_ = false;
  if (!stats_.Init(this, &data_->header.stats))
    return false;

  disabled_ = !rankings_.Init(this, new_eviction_);
  eviction_.Init(this);

  return !disabled_;
}

BackendImpl::~BackendImpl() {
  Trace("Backend destructor");
  if (!init_)
    return;

  if (data_)
    data_->header.crash = 0;

  timer_.Stop();

  WaitForPendingIO(&num_pending_io_);
  DCHECK(!num_refs_);
}

// ------------------------------------------------------------------------

int32 BackendImpl::GetEntryCount() const {
  if (!index_)
    return 0;
  // num_entries includes entries already evicted.
  int32 not_deleted = data_->header.num_entries -
                      data_->header.lru.sizes[Rankings::DELETED];

  if (not_deleted < 0) {
    NOTREACHED();
    not_deleted = 0;
  }

  return not_deleted;
}

bool BackendImpl::OpenEntry(const std::string& key, Entry** entry) {
  if (disabled_)
    return false;

  Time start = Time::Now();
  uint32 hash = Hash(key);

  EntryImpl* cache_entry = MatchEntry(key, hash, false);
  if (!cache_entry) {
    stats_.OnEvent(Stats::OPEN_MISS);
    return false;
  }

  if (ENTRY_NORMAL != cache_entry->entry()->Data()->state) {
    // The entry was already evicted.
    cache_entry->Release();
    stats_.OnEvent(Stats::OPEN_MISS);
    return false;
  }

  eviction_.OnOpenEntry(cache_entry);
  DCHECK(entry);
  *entry = cache_entry;

  CACHE_UMA(AGE_MS, "OpenTime", GetSizeGroup(), start);
  stats_.OnEvent(Stats::OPEN_HIT);
  return true;
}

bool BackendImpl::CreateEntry(const std::string& key, Entry** entry) {
  if (disabled_ || key.empty())
    return false;

  DCHECK(entry);
  *entry = NULL;

  Time start = Time::Now();
  uint32 hash = Hash(key);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    // We have an entry already. It could be the one we are looking for, or just
    // a hash conflict.
    EntryImpl* old_entry = MatchEntry(key, hash, false);
    if (old_entry)
      return ResurrectEntry(old_entry, entry);

    EntryImpl* parent_entry = MatchEntry(key, hash, true);
    if (!parent_entry) {
      NOTREACHED();
      return false;
    }
    parent.swap(&parent_entry);
  }

  int num_blocks;
  size_t key1_len = sizeof(EntryStore) - offsetof(EntryStore, key);
  if (key.size() < key1_len ||
      key.size() > static_cast<size_t>(kMaxInternalKeyLength))
    num_blocks = 1;
  else
    num_blocks = static_cast<int>((key.size() - key1_len) / 256 + 2);

  if (!block_files_.CreateBlock(BLOCK_256, num_blocks, &entry_address)) {
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return false;
  }

  Addr node_address(0);
  if (!block_files_.CreateBlock(RANKINGS, 1, &node_address)) {
    block_files_.DeleteBlock(entry_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return false;
  }

  scoped_refptr<EntryImpl> cache_entry(new EntryImpl(this, entry_address));
  IncreaseNumRefs();

  if (!cache_entry->CreateEntry(node_address, key, hash)) {
    block_files_.DeleteBlock(entry_address, false);
    block_files_.DeleteBlock(node_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return false;
  }

  // We are not failing the operation; let's add this to the map.
  open_entries_[entry_address.value()] = cache_entry;

  if (parent.get())
    parent->SetNextAddress(entry_address);

  block_files_.GetFile(entry_address)->Store(cache_entry->entry());
  block_files_.GetFile(node_address)->Store(cache_entry->rankings());

  IncreaseNumEntries();
  eviction_.OnCreateEntry(cache_entry);
  if (!parent.get())
    data_->table[hash & mask_] = entry_address.value();

  cache_entry.swap(reinterpret_cast<EntryImpl**>(entry));

  CACHE_UMA(AGE_MS, "CreateTime", GetSizeGroup(), start);
  stats_.OnEvent(Stats::CREATE_HIT);
  Trace("create entry hit ");
  return true;
}

bool BackendImpl::DoomEntry(const std::string& key) {
  if (disabled_)
    return false;

  Entry* entry;
  if (!OpenEntry(key, &entry))
    return false;

  // Note that you'd think you could just pass &entry_impl to OpenEntry,
  // but that triggers strict aliasing problems with gcc.
  EntryImpl* entry_impl = reinterpret_cast<EntryImpl*>(entry);
  entry_impl->Doom();
  entry_impl->Release();
  return true;
}

bool BackendImpl::DoomAllEntries() {
  if (!num_refs_) {
    PrepareForRestart();
    DeleteCache(path_.c_str(), false);
    return Init();
  } else {
    if (disabled_)
      return false;

    eviction_.TrimCache(true);
    stats_.OnEvent(Stats::DOOM_CACHE);
    return true;
  }
}

bool BackendImpl::DoomEntriesBetween(const Time initial_time,
                                     const Time end_time) {
  if (end_time.is_null())
    return DoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  if (disabled_)
    return false;

  Entry* node, *next;
  void* iter = NULL;
  if (!OpenNextEntry(&iter, &next))
    return true;

  while (next) {
    node = next;
    if (!OpenNextEntry(&iter, &next))
      next = NULL;

    if (node->GetLastUsed() >= initial_time &&
        node->GetLastUsed() < end_time) {
      node->Doom();
    } else if (node->GetLastUsed() < initial_time) {
      if (next)
        next->Close();
      next = NULL;
      EndEnumeration(&iter);
    }

    node->Close();
  }

  return true;
}

// We use OpenNextEntry to retrieve elements from the cache, until we get
// entries that are too old.
bool BackendImpl::DoomEntriesSince(const Time initial_time) {
  if (disabled_)
    return false;

  for (;;) {
    Entry* entry;
    void* iter = NULL;
    if (!OpenNextEntry(&iter, &entry))
      return true;

    if (initial_time > entry->GetLastUsed()) {
      entry->Close();
      EndEnumeration(&iter);
      return true;
    }

    entry->Doom();
    entry->Close();
    EndEnumeration(&iter);  // Dooming the entry invalidates the iterator.
  }
}

bool BackendImpl::OpenNextEntry(void** iter, Entry** next_entry) {
  return OpenFollowingEntry(true, iter, next_entry);
}

void BackendImpl::EndEnumeration(void** iter) {
  scoped_ptr<Rankings::Iterator> iterator(
      reinterpret_cast<Rankings::Iterator*>(*iter));
  *iter = NULL;
}

void BackendImpl::GetStats(StatsItems* stats) {
  if (disabled_)
    return;

  std::pair<std::string, std::string> item;

  item.first = "Entries";
  item.second = StringPrintf("%d", data_->header.num_entries);
  stats->push_back(item);

  item.first = "Pending IO";
  item.second = StringPrintf("%d", num_pending_io_);
  stats->push_back(item);

  item.first = "Max size";
  item.second = StringPrintf("%d", max_size_);
  stats->push_back(item);

  item.first = "Current size";
  item.second = StringPrintf("%d", data_->header.num_bytes);
  stats->push_back(item);

  stats_.GetItems(stats);
}

// ------------------------------------------------------------------------

bool BackendImpl::SetMaxSize(int max_bytes) {
  COMPILE_ASSERT(sizeof(max_bytes) == sizeof(max_size_), unsupported_int_model);
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  // Avoid a DCHECK later on.
  if (max_bytes >= kint32max - kint32max / 10)
    max_bytes = kint32max - kint32max / 10 - 1;

  user_flags_ |= kMaxSize;
  max_size_ = max_bytes;
  return true;
}

void BackendImpl::SetType(net::CacheType type) {
  DCHECK(type != net::MEMORY_CACHE);
  cache_type_ = type;
}

std::wstring BackendImpl::GetFileName(Addr address) const {
  if (!address.is_separate_file() || !address.is_initialized()) {
    NOTREACHED();
    return std::wstring();
  }

  std::wstring name(path_);
  std::wstring tmp = StringPrintf(L"f_%06x", address.FileNumber());
  file_util::AppendToPath(&name, tmp);
  return name;
}

MappedFile* BackendImpl::File(Addr address) {
  if (disabled_)
    return NULL;
  return block_files_.GetFile(address);
}

bool BackendImpl::CreateExternalFile(Addr* address) {
  int file_number = data_->header.last_file + 1;
  Addr file_address(0);
  bool success = false;
  for (int i = 0; i < 0x0fffffff; i++, file_number++) {
    if (!file_address.SetFileNumber(file_number)) {
      file_number = 1;
      continue;
    }
    std::wstring name = GetFileName(file_address);
    int flags = base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_WRITE |
                base::PLATFORM_FILE_CREATE |
                base::PLATFORM_FILE_EXCLUSIVE_WRITE;
    scoped_refptr<disk_cache::File> file(new disk_cache::File(
        base::CreatePlatformFile(name.c_str(), flags, NULL)));
    if (!file->IsValid())
      continue;

    success = true;
    break;
  }

  DCHECK(success);
  if (!success)
    return false;

  data_->header.last_file = file_number;
  address->set_value(file_address.value());
  return true;
}

bool BackendImpl::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  return block_files_.CreateBlock(block_type, block_count, block_address);
}

void BackendImpl::DeleteBlock(Addr block_address, bool deep) {
  block_files_.DeleteBlock(block_address, deep);
}

LruData* BackendImpl::GetLruData() {
  return &data_->header.lru;
}

void BackendImpl::UpdateRank(EntryImpl* entry, bool modified) {
  if (!read_only_) {
    eviction_.UpdateRank(entry, modified);
  }
}

void BackendImpl::RecoveredEntry(CacheRankingsBlock* rankings) {
  Addr address(rankings->Data()->contents);
  EntryImpl* cache_entry = NULL;
  bool dirty;
  if (NewEntry(address, &cache_entry, &dirty))
    return;

  uint32 hash = cache_entry->GetHash();
  cache_entry->Release();

  // Anything on the table means that this entry is there.
  if (data_->table[hash & mask_])
    return;

  data_->table[hash & mask_] = address.value();
}

void BackendImpl::InternalDoomEntry(EntryImpl* entry) {
  uint32 hash = entry->GetHash();
  std::string key = entry->GetKey();
  EntryImpl* parent_entry = MatchEntry(key, hash, true);
  CacheAddr child(entry->GetNextAddress());

  Trace("Doom entry 0x%p", entry);

  eviction_.OnDoomEntry(entry);
  entry->InternalDoom();

  if (parent_entry) {
    parent_entry->SetNextAddress(Addr(child));
    parent_entry->Release();
  } else {
    data_->table[hash & mask_] = child;
  }

  if (!new_eviction_) {
    DecreaseNumEntries();
  }

  stats_.OnEvent(Stats::DOOM_ENTRY);
}

// An entry may be linked on the DELETED list for a while after being doomed.
// This function is called when we want to remove it.
void BackendImpl::RemoveEntry(EntryImpl* entry) {
  if (!new_eviction_)
    return;

  DCHECK(ENTRY_NORMAL != entry->entry()->Data()->state);

  Trace("Remove entry 0x%p", entry);
  eviction_.OnDestroyEntry(entry);
  DecreaseNumEntries();
}

void BackendImpl::CacheEntryDestroyed(Addr address) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end())
    open_entries_.erase(it);
  DecreaseNumRefs();
}

bool BackendImpl::IsOpen(CacheRankingsBlock* rankings) const {
  DCHECK(rankings->HasData());
  EntriesMap::const_iterator it =
      open_entries_.find(rankings->Data()->contents);
  if (it != open_entries_.end()) {
    // We have this entry in memory.
    return rankings->Data()->pointer == it->second;
  }

  return false;
}

int32 BackendImpl::GetCurrentEntryId() const {
  return data_->header.this_id;
}

int BackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

void BackendImpl::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (disabled_ || old_size == new_size)
    return;
  if (old_size > new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);

  // Update the usage statistics.
  stats_.ModifyStorageStats(old_size, new_size);
}

void BackendImpl::TooMuchStorageRequested(int32 size) {
  stats_.ModifyStorageStats(0, size);
}

bool BackendImpl::IsLoaded() const {
  CACHE_UMA(COUNTS, "PendingIO", GetSizeGroup(), num_pending_io_);
  return num_pending_io_ > 10;
}

std::string BackendImpl::HistogramName(const char* name, int experiment) const {
  if (!experiment)
    return StringPrintf("DiskCache.%d.%s", cache_type_, name);
  return StringPrintf("DiskCache.%d.%s_%d", cache_type_, name, experiment);
}

int BackendImpl::GetSizeGroup() const {
  if (disabled_)
    return 0;

  // We want to report times grouped by the current cache size (50 MB groups).
  int group = data_->header.num_bytes / (50 * 1024 * 1024);
  if (group > 6)
    group = 6;  // Limit the number of groups, just in case.
  return group;
}

// We want to remove biases from some histograms so we only send data once per
// week.
bool BackendImpl::ShouldReportAgain() {
  if (uma_report_)
    return uma_report_ == 2;

  uma_report_++;
  int64 last_report = stats_.GetCounter(Stats::LAST_REPORT);
  Time last_time = Time::FromInternalValue(last_report);
  if (!last_report || (Time::Now() - last_time).InDays() >= 7) {
    stats_.SetCounter(Stats::LAST_REPORT, Time::Now().ToInternalValue());
    uma_report_++;
    return true;
  }
  return false;
}

void BackendImpl::FirstEviction() {
  DCHECK(data_->header.create_time);

  Time create_time = Time::FromInternalValue(data_->header.create_time);
  CACHE_UMA(AGE, "FillupAge", 0, create_time);

  int64 use_hours = stats_.GetCounter(Stats::TIMER) / 120;
  CACHE_UMA(HOURS, "FillupTime", 0, static_cast<int>(use_hours));
  CACHE_UMA(PERCENTAGE, "FirstHitRatio", 0, stats_.GetHitRatio());

  int avg_size = data_->header.num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "FirstEntrySize", 0, avg_size);

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / data_->header.num_bytes;
  CACHE_UMA(PERCENTAGE, "FirstLargeEntriesRatio", 0, large_ratio);

  if (data_->header.experiment == 8) {
    CACHE_UMA(PERCENTAGE, "FirstResurrectRatio", 8, stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "FirstNoUseRatio", 8,
              data_->header.lru.sizes[0] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "FirstLowUseRatio", 8,
              data_->header.lru.sizes[1] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "FirstHighUseRatio", 8,
              data_->header.lru.sizes[2] * 100 / data_->header.num_entries);
  }

  stats_.ResetRatios();
}

void BackendImpl::CriticalError(int error) {
  LOG(ERROR) << "Critical error found " << error;
  if (disabled_)
    return;

  LogStats();
  ReportError(error);

  // Setting the index table length to an invalid value will force re-creation
  // of the cache files.
  data_->header.table_len = 1;
  disabled_ = true;

  if (!num_refs_)
    RestartCache();
}

void BackendImpl::ReportError(int error) {
  // We transmit positive numbers, instead of direct error codes.
  DCHECK(error <= 0);
  CACHE_UMA(CACHE_ERROR, "Error", 0, error * -1);
}

void BackendImpl::OnEvent(Stats::Counters an_event) {
  stats_.OnEvent(an_event);
}

void BackendImpl::OnStatsTimer() {
  stats_.OnEvent(Stats::TIMER);
  int64 time = stats_.GetCounter(Stats::TIMER);
  int64 current = stats_.GetCounter(Stats::OPEN_ENTRIES);

  // OPEN_ENTRIES is a sampled average of the number of open entries, avoiding
  // the bias towards 0.
  if (num_refs_ && (current != num_refs_)) {
    int64 diff = (num_refs_ - current) / 50;
    if (!diff)
      diff = num_refs_ > current ? 1 : -1;
    current = current + diff;
    stats_.SetCounter(Stats::OPEN_ENTRIES, current);
    stats_.SetCounter(Stats::MAX_ENTRIES, max_refs_);
  }

  CACHE_UMA(COUNTS, "NumberOfReferences", 0, num_refs_);

  if (!data_)
    first_timer_ = false;
  if (first_timer_) {
    first_timer_ = false;
    if (ShouldReportAgain())
      ReportStats();
  }

  // Save stats to disk at 5 min intervals.
  if (time % 10 == 0)
    stats_.Store();
}

void BackendImpl::IncrementIoCount() {
  num_pending_io_++;
}

void BackendImpl::DecrementIoCount() {
  num_pending_io_--;
}

void BackendImpl::SetUnitTestMode() {
  user_flags_ |= kUnitTestMode;
  unit_test_ = true;
}

void BackendImpl::SetUpgradeMode() {
  user_flags_ |= kUpgradeMode;
  read_only_ = true;
}

void BackendImpl::SetNewEviction() {
  user_flags_ |= kNewEviction;
  new_eviction_ = true;
}

void BackendImpl::ClearRefCountForTest() {
  num_refs_ = 0;
}

int BackendImpl::SelfCheck() {
  if (!init_) {
    LOG(ERROR) << "Init failed";
    return ERR_INIT_FAILED;
  }

  int num_entries = rankings_.SelfCheck();
  if (num_entries < 0) {
    LOG(ERROR) << "Invalid rankings list, error " << num_entries;
    return num_entries;
  }

  if (num_entries != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return CheckAllEntries();
}

bool BackendImpl::OpenPrevEntry(void** iter, Entry** prev_entry) {
  return OpenFollowingEntry(false, iter, prev_entry);
}

// ------------------------------------------------------------------------

// We just created a new file so we're going to write the header and set the
// file length to include the hash table (zero filled).
bool BackendImpl::CreateBackingStore(disk_cache::File* file) {
  AdjustMaxCacheSize(0);

  IndexHeader header;
  header.table_len = DesiredIndexTableLen(max_size_);

  // We need file version 2.1 for the new eviction algorithm.
  if (new_eviction_)
    header.version = 0x20001;

  header.create_time = Time::Now().ToInternalValue();

  if (!file->Write(&header, sizeof(header), 0))
    return false;

  return file->SetLength(GetIndexSize(header.table_len));
}

bool BackendImpl::InitBackingStore(bool* file_created) {
  file_util::CreateDirectory(path_);

  std::wstring index_name(path_);
  file_util::AppendToPath(&index_name, kIndexName);

  int flags = base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_OPEN_ALWAYS |
              base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  scoped_refptr<disk_cache::File> file(new disk_cache::File(
      base::CreatePlatformFile(index_name.c_str(), flags, file_created)));

  if (!file->IsValid())
    return false;

  bool ret = true;
  if (*file_created)
    ret = CreateBackingStore(file);

  file = NULL;
  if (!ret)
    return false;

  index_ = new MappedFile();
  data_ = reinterpret_cast<Index*>(index_->Init(index_name, 0));
  if (!data_) {
    LOG(ERROR) << "Unable to map Index file";
    return false;
  }
  return true;
}

// The maximum cache size will be either set explicitly by the caller, or
// calculated by this code.
void BackendImpl::AdjustMaxCacheSize(int table_len) {
  if (max_size_)
    return;

  // If table_len is provided, the index file exists.
  DCHECK(!table_len || data_->header.magic);

  // The user is not setting the size, let's figure it out.
  int64 available = base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (available < 0) {
    max_size_ = kDefaultCacheSize;
    return;
  }

  if (table_len)
    available += data_->header.num_bytes;

  max_size_ = PreferedCacheSize(available);

  // Let's not use more than the default size while we tune-up the performance
  // of bigger caches. TODO(rvargas): remove this limit.
  if (max_size_ > kDefaultCacheSize * 4)
    max_size_ = kDefaultCacheSize * 4;

  if (!table_len)
    return;

  // If we already have a table, adjust the size to it.
  int current_max_size = MaxStorageSizeForTable(table_len);
  if (max_size_ > current_max_size)
    max_size_= current_max_size;
}

void BackendImpl::RestartCache() {
  PrepareForRestart();
  DelayedCacheCleanup(path_);

  int64 errors = stats_.GetCounter(Stats::FATAL_ERROR);

  // Don't call Init() if directed by the unit test: we are simulating a failure
  // trying to re-enable the cache.
  if (unit_test_)
    init_ = true;  // Let the destructor do proper cleanup.
  else if (Init())
    stats_.SetCounter(Stats::FATAL_ERROR, errors + 1);
}

void BackendImpl::PrepareForRestart() {
  // Reset the mask_ if it was not given by the user.
  if (!(user_flags_ & kMask))
    mask_ = 0;

  if (!(user_flags_ & kNewEviction))
    new_eviction_ = false;

  data_->header.crash = 0;
  index_ = NULL;
  data_ = NULL;
  block_files_.CloseFiles();
  rankings_.Reset();
  init_ = false;
  restarted_ = true;
}

int BackendImpl::NewEntry(Addr address, EntryImpl** entry, bool* dirty) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end()) {
    // Easy job. This entry is already in memory.
    EntryImpl* this_entry = it->second;
    this_entry->AddRef();
    *entry = this_entry;
    *dirty = false;
    return 0;
  }

  scoped_refptr<EntryImpl> cache_entry(new EntryImpl(this, address));
  IncreaseNumRefs();
  *entry = NULL;

  if (!address.is_initialized() || address.is_separate_file() ||
      address.file_type() != BLOCK_256) {
    LOG(WARNING) << "Wrong entry address.";
    return ERR_INVALID_ADDRESS;
  }

  if (!cache_entry->entry()->Load())
    return ERR_READ_FAILURE;

  if (!cache_entry->SanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    return ERR_INVALID_ENTRY;
  }

  if (!cache_entry->LoadNodeAddress())
    return ERR_READ_FAILURE;

  *dirty = cache_entry->IsDirty(GetCurrentEntryId());

  // Prevent overwriting the dirty flag on the destructor.
  cache_entry->ClearDirtyFlag();

  if (!rankings_.SanityCheck(cache_entry->rankings(), false))
    return ERR_INVALID_LINKS;

  // We only add clean entries to the map.
  if (!*dirty)
    open_entries_[address.value()] = cache_entry;

  cache_entry.swap(entry);
  return 0;
}

EntryImpl* BackendImpl::MatchEntry(const std::string& key, uint32 hash,
                                   bool find_parent) {
  Addr address(data_->table[hash & mask_]);
  scoped_refptr<EntryImpl> cache_entry, parent_entry;
  EntryImpl* tmp = NULL;
  bool found = false;

  for (;;) {
    if (disabled_)
      break;

    if (!address.is_initialized()) {
      if (find_parent)
        found = true;
      break;
    }

    bool dirty;
    int error = NewEntry(address, &tmp, &dirty);
    cache_entry.swap(&tmp);

    if (error || dirty) {
      // This entry is dirty on disk (it was not properly closed): we cannot
      // trust it.
      Addr child(0);
      if (!error)
        child.set_value(cache_entry->GetNextAddress());

      if (parent_entry) {
        parent_entry->SetNextAddress(child);
        parent_entry = NULL;
      } else {
        data_->table[hash & mask_] = child.value();
      }

      if (!error) {
        // It is important to call DestroyInvalidEntry after removing this
        // entry from the table.
        DestroyInvalidEntry(address, cache_entry);
        cache_entry = NULL;
      } else {
        Trace("NewEntry failed on MatchEntry 0x%x", address.value());
      }

      // Restart the search.
      address.set_value(data_->table[hash & mask_]);
      continue;
    }

    if (cache_entry->IsSameEntry(key, hash)) {
      if (!cache_entry->Update())
        cache_entry = NULL;
      found = true;
      break;
    }
    if (!cache_entry->Update())
      cache_entry = NULL;
    parent_entry = cache_entry;
    cache_entry = NULL;
    if (!parent_entry)
      break;

    address.set_value(parent_entry->GetNextAddress());
  }

  if (parent_entry && (!find_parent || !found))
    parent_entry = NULL;

  if (cache_entry && (find_parent || !found))
    cache_entry = NULL;

  find_parent ? parent_entry.swap(&tmp) : cache_entry.swap(&tmp);
  return tmp;
}

// This is the actual implementation for OpenNextEntry and OpenPrevEntry.
bool BackendImpl::OpenFollowingEntry(bool forward, void** iter,
                                     Entry** next_entry) {
  if (disabled_)
    return false;

  DCHECK(iter);
  DCHECK(next_entry);
  *next_entry = NULL;

  const int kListsToSearch = 3;
  scoped_refptr<EntryImpl> entries[kListsToSearch];
  scoped_ptr<Rankings::Iterator> iterator(
      reinterpret_cast<Rankings::Iterator*>(*iter));
  *iter = NULL;

  if (!iterator.get()) {
    iterator.reset(new Rankings::Iterator(&rankings_));
    bool ret = false;

    // Get an entry from each list.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      ret |= OpenFollowingEntryFromList(forward, static_cast<Rankings::List>(i),
                                        &iterator->nodes[i], &temp);
      entries[i].swap(&temp);  // The entry was already addref'd.
    }
    if (!ret)
      return false;
  } else {
    // Get the next entry from the last list, and the actual entries for the
    // elements on the other lists.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      if (iterator->list == i) {
          OpenFollowingEntryFromList(forward, iterator->list,
                                     &iterator->nodes[i], &temp);
      } else {
        temp = GetEnumeratedEntry(iterator->nodes[i]);
      }

      entries[i].swap(&temp);  // The entry was already addref'd.
    }
  }

  int newest = -1;
  int oldest = -1;
  Time access_times[kListsToSearch];
  for (int i = 0; i < kListsToSearch; i++) {
    if (entries[i].get()) {
      access_times[i] = entries[i]->GetLastUsed();
      if (newest < 0) {
        DCHECK(oldest < 0);
        newest = oldest = i;
        continue;
      }
      if (access_times[i] > access_times[newest])
        newest = i;
      if (access_times[i] < access_times[oldest])
        oldest = i;
    }
  }

  if (newest < 0 || oldest < 0)
    return false;

  if (forward) {
    entries[newest].swap(reinterpret_cast<EntryImpl**>(next_entry));
    iterator->list = static_cast<Rankings::List>(newest);
  } else {
    entries[oldest].swap(reinterpret_cast<EntryImpl**>(next_entry));
    iterator->list = static_cast<Rankings::List>(oldest);
  }

  *iter = iterator.release();
  return true;
}

bool BackendImpl::OpenFollowingEntryFromList(bool forward, Rankings::List list,
                                             CacheRankingsBlock** from_entry,
                                             EntryImpl** next_entry) {
  if (disabled_)
    return false;

  if (!new_eviction_ && Rankings::NO_USE != list)
    return false;

  Rankings::ScopedRankingsBlock rankings(&rankings_, *from_entry);
  CacheRankingsBlock* next_block = forward ?
      rankings_.GetNext(rankings.get(), list) :
      rankings_.GetPrev(rankings.get(), list);
  Rankings::ScopedRankingsBlock next(&rankings_, next_block);
  *from_entry = NULL;

  *next_entry = GetEnumeratedEntry(next.get());
  if (!*next_entry)
    return false;

  *from_entry = next.release();
  return true;
}

EntryImpl* BackendImpl::GetEnumeratedEntry(CacheRankingsBlock* next) {
  if (!next)
    return NULL;

  scoped_refptr<EntryImpl> entry;
  EntryImpl* temp = NULL;

  if (next->Data()->pointer) {
    entry = reinterpret_cast<EntryImpl*>(next->Data()->pointer);
    entry.swap(&temp);
    return temp;
  }

  bool dirty;
  if (NewEntry(Addr(next->Data()->contents), &temp, &dirty))
    return NULL;
  entry.swap(&temp);

  if (dirty) {
    // We cannot trust this entry. Call MatchEntry to go through the regular
    // path and take the appropriate action.
    std::string key = entry->GetKey();
    uint32 hash = entry->GetHash();
    entry = NULL;  // Release the entry.
    temp = MatchEntry(key, hash, false);
    if (temp)
      temp->Release();

    return NULL;
  }
  if (!entry->Update())
    return NULL;

  entry.swap(&temp);
  return temp;
}

bool BackendImpl::ResurrectEntry(EntryImpl* deleted_entry, Entry** entry) {
  if (ENTRY_NORMAL == deleted_entry->entry()->Data()->state) {
    deleted_entry->Release();
    stats_.OnEvent(Stats::CREATE_MISS);
    Trace("create entry miss ");
    return false;
  }

  // We are attempting to create an entry and found out that the entry was
  // previously deleted.

  eviction_.OnCreateEntry(deleted_entry);
  *entry = deleted_entry;

  stats_.OnEvent(Stats::CREATE_HIT);
  Trace("Resurrect entry hit ");
  return true;
}

void BackendImpl::DestroyInvalidEntry(Addr address, EntryImpl* entry) {
  LOG(WARNING) << "Destroying invalid entry.";
  Trace("Destroying invalid entry 0x%p", entry);

  entry->SetPointerForInvalidEntry(GetCurrentEntryId());

  eviction_.OnDoomEntry(entry);
  entry->InternalDoom();

  if (!new_eviction_)
    DecreaseNumEntries();
  stats_.OnEvent(Stats::INVALID_ENTRY);
}

void BackendImpl::AddStorageSize(int32 bytes) {
  data_->header.num_bytes += bytes;
  DCHECK(data_->header.num_bytes >= 0);

  if (data_->header.num_bytes > max_size_)
    eviction_.TrimCache(false);
}

void BackendImpl::SubstractStorageSize(int32 bytes) {
  data_->header.num_bytes -= bytes;
  DCHECK(data_->header.num_bytes >= 0);
}

void BackendImpl::IncreaseNumRefs() {
  num_refs_++;
  if (max_refs_ < num_refs_)
    max_refs_ = num_refs_;
}

void BackendImpl::DecreaseNumRefs() {
  DCHECK(num_refs_);
  num_refs_--;

  if (!num_refs_ && disabled_)
    RestartCache();
}

void BackendImpl::IncreaseNumEntries() {
  data_->header.num_entries++;
  DCHECK(data_->header.num_entries > 0);
}

void BackendImpl::DecreaseNumEntries() {
  data_->header.num_entries--;
  if (data_->header.num_entries < 0) {
    NOTREACHED();
    data_->header.num_entries = 0;
  }
}

void BackendImpl::LogStats() {
  StatsItems stats;
  GetStats(&stats);

  for (size_t index = 0; index < stats.size(); index++) {
    LOG(INFO) << stats[index].first << ": " << stats[index].second;
  }
}

void BackendImpl::ReportStats() {
  CACHE_UMA(COUNTS, "Entries", 0, data_->header.num_entries);
  CACHE_UMA(COUNTS, "Size", 0, data_->header.num_bytes / (1024 * 1024));
  CACHE_UMA(COUNTS, "MaxSize", 0, max_size_ / (1024 * 1024));

  CACHE_UMA(COUNTS, "AverageOpenEntries", 0,
            static_cast<int>(stats_.GetCounter(Stats::OPEN_ENTRIES)));
  CACHE_UMA(COUNTS, "MaxOpenEntries", 0,
            static_cast<int>(stats_.GetCounter(Stats::MAX_ENTRIES)));
  stats_.SetCounter(Stats::MAX_ENTRIES, 0);

  if (!data_->header.create_time || !data_->header.lru.filled)
    return;

  // This is an up to date client that will report FirstEviction() data. After
  // that event, start reporting this:

  int64 total_hours = stats_.GetCounter(Stats::TIMER) / 120;
  CACHE_UMA(HOURS, "TotalTime", 0, static_cast<int>(total_hours));

  int64 use_hours = stats_.GetCounter(Stats::LAST_REPORT_TIMER) / 120;
  stats_.SetCounter(Stats::LAST_REPORT_TIMER, stats_.GetCounter(Stats::TIMER));

  // We may see users with no use_hours at this point if this is the first time
  // we are running this code.
  if (use_hours)
    use_hours = total_hours - use_hours;

  if (!use_hours || !GetEntryCount() || !data_->header.num_bytes)
    return;

  CACHE_UMA(HOURS, "UseTime", 0, static_cast<int>(use_hours));
  CACHE_UMA(PERCENTAGE, "HitRatio", data_->header.experiment,
            stats_.GetHitRatio());

  int64 trim_rate = stats_.GetCounter(Stats::TRIM_ENTRY) / use_hours;
  CACHE_UMA(COUNTS, "TrimRate", 0, static_cast<int>(trim_rate));

  int avg_size = data_->header.num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "EntrySize", data_->header.experiment, avg_size);

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / data_->header.num_bytes;
  CACHE_UMA(PERCENTAGE, "LargeEntriesRatio", 0, large_ratio);

  if (new_eviction_) {
    CACHE_UMA(PERCENTAGE, "ResurrectRatio", data_->header.experiment,
              stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "NoUseRatio", data_->header.experiment,
              data_->header.lru.sizes[0] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "LowUseRatio", data_->header.experiment,
              data_->header.lru.sizes[1] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "HighUseRatio", data_->header.experiment,
              data_->header.lru.sizes[2] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "DeletedRatio", data_->header.experiment,
              data_->header.lru.sizes[4] * 100 / data_->header.num_entries);
  }

  stats_.ResetRatios();
  stats_.SetCounter(Stats::TRIM_ENTRY, 0);
}

void BackendImpl::UpgradeTo2_1() {
  // 2.1 is basically the same as 2.0, except that new fields are actually
  // updated by the new eviction algorithm.
  DCHECK(0x20000 == data_->header.version);
  data_->header.version = 0x20001;
  data_->header.lru.sizes[Rankings::NO_USE] = data_->header.num_entries;
}

bool BackendImpl::CheckIndex() {
  DCHECK(data_);

  size_t current_size = index_->GetLength();
  if (current_size < sizeof(Index)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  if (new_eviction_) {
    // We support versions 2.0 and 2.1, upgrading 2.0 to 2.1.
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion >> 16 != data_->header.version >> 16) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
    if (kCurrentVersion == data_->header.version) {
      // We need file version 2.1 for the new eviction algorithm.
      UpgradeTo2_1();
    }
  } else {
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion != data_->header.version) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
  }

  if (!data_->header.table_len) {
    LOG(ERROR) << "Invalid table size";
    return false;
  }

  if (current_size < GetIndexSize(data_->header.table_len) ||
      data_->header.table_len & (kBaseTableLen - 1)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  AdjustMaxCacheSize(data_->header.table_len);

  // We need to avoid integer overflows.
  DCHECK(max_size_ < kint32max - kint32max / 10);
  if (data_->header.num_bytes < 0 ||
      data_->header.num_bytes > max_size_ + max_size_ / 10) {
    LOG(ERROR) << "Invalid cache (current) size";
    return false;
  }

  if (data_->header.num_entries < 0) {
    LOG(ERROR) << "Invalid number of entries";
    return false;
  }

  if (!mask_)
    mask_ = data_->header.table_len - 1;

  return true;
}

int BackendImpl::CheckAllEntries() {
  int num_dirty = 0;
  int num_entries = 0;
  DCHECK(mask_ < kuint32max);
  for (int i = 0; i <= static_cast<int>(mask_); i++) {
    Addr address(data_->table[i]);
    if (!address.is_initialized())
      continue;
    for (;;) {
      bool dirty;
      EntryImpl* tmp;
      int ret = NewEntry(address, &tmp, &dirty);
      if (ret)
        return ret;
      scoped_refptr<EntryImpl> cache_entry;
      cache_entry.swap(&tmp);

      if (dirty)
        num_dirty++;
      else if (CheckEntry(cache_entry.get()))
        num_entries++;
      else
        return ERR_INVALID_ENTRY;

      address.set_value(cache_entry->GetNextAddress());
      if (!address.is_initialized())
        break;
    }
  }

  if (num_entries + num_dirty != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return num_dirty;
}

bool BackendImpl::CheckEntry(EntryImpl* cache_entry) {
  RankingsNode* rankings = cache_entry->rankings()->Data();
  return !rankings->pointer;
}

}  // namespace disk_cache
