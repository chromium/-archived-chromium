// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/backend_impl.h"

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/timer.h"
#include "base/worker_pool.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/errors.h"
#include "net/disk_cache/hash.h"
#include "net/disk_cache/file.h"

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
  std::wstring path(full_path);
  file_util::TrimTrailingSeparator(&path);

  std::wstring name = file_util::GetFilenameFromPath(path);
  file_util::TrimFilename(&path);

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

// Sets |stored_value| for the current experiment.
void InitExperiment(int* stored_value) {
  if (*stored_value)
    return;

  srand(static_cast<int>(Time::Now().ToInternalValue()));
  int option = rand() % 10;

  // Values used by the current experiment are 1 through 4.
  if (option > 2) {
    // 70% will be here.
    *stored_value = 1;
  } else {
    *stored_value = option + 2;
  }
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
                            int max_bytes) {
  BackendImpl* cache = new BackendImpl(full_path);
  cache->SetMaxSize(max_bytes);
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
  if (cache->Init())
    return cache;

  delete cache;
  LOG(ERROR) << "Unable to create cache";
  return NULL;
}

// ------------------------------------------------------------------------

bool BackendImpl::Init() {
  DCHECK(!init_);
  if (init_)
    return false;

  bool create_files = false;
  if (!InitBackingStore(&create_files)) {
    ReportError(ERR_STORAGE_ERROR);
    return false;
  }

  num_refs_ = num_pending_io_ = max_refs_ = 0;

  if (!restarted_) {
    // Create a recurrent timer of 30 secs.
    int timer_delay = unit_test_ ? 1000 : 30000;
    timer_.Start(TimeDelta::FromMilliseconds(timer_delay), this,
                 &BackendImpl::OnStatsTimer);
  }

  init_ = true;
  if (data_)
    InitExperiment(&data_->header.experiment);

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

  disabled_ = !rankings_.Init(this);
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
  return data_->header.num_entries;
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

  eviction_.OnOpenEntry(cache_entry);
  DCHECK(entry);
  *entry = cache_entry;

  UMA_HISTOGRAM_TIMES(L"DiskCache.OpenTime", Time::Now() - start);
  stats_.OnEvent(Stats::OPEN_HIT);
  return true;
}

bool BackendImpl::CreateEntry(const std::string& key, Entry** entry) {
  if (disabled_ || key.empty())
    return false;

  Time start = Time::Now();
  uint32 hash = Hash(key);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    EntryImpl* parent_entry = MatchEntry(key, hash, true);
    if (!parent_entry) {
      stats_.OnEvent(Stats::CREATE_MISS);
      Trace("create entry miss ");
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

  if (parent.get())
    parent->SetNextAddress(entry_address);

  block_files_.GetFile(entry_address)->Store(cache_entry->entry());
  block_files_.GetFile(node_address)->Store(cache_entry->rankings());

  data_->header.num_entries++;
  DCHECK(data_->header.num_entries > 0);
  eviction_.OnCreateEntry(cache_entry);
  if (!parent.get())
    data_->table[hash & mask_] = entry_address.value();

  DCHECK(entry);
  *entry = NULL;
  cache_entry.swap(reinterpret_cast<EntryImpl**>(entry));

  UMA_HISTOGRAM_TIMES(L"DiskCache.CreateTime", Time::Now() - start);
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
  Rankings::ScopedRankingsBlock rankings(&rankings_,
      reinterpret_cast<CacheRankingsBlock*>(*iter));
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

  max_size_ = max_bytes;
  return true;
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

  data_->header.num_entries--;
  DCHECK(data_->header.num_entries >= 0);
  stats_.OnEvent(Stats::DOOM_ENTRY);
}

void BackendImpl::CacheEntryDestroyed() {
  DecreaseNumRefs();
}

int32 BackendImpl::GetCurrentEntryId() {
  return data_->header.this_id;
}

int BackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

void BackendImpl::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (disabled_)
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
  static LinearHistogram counter(L"DiskCache.Error", 0, 49, 50);
  counter.SetFlags(kUmaTargetedHistogramFlag);

  // We transmit positive numbers, instead of direct error codes.
  DCHECK(error <= 0);
  counter.Add(error * -1);
}

void BackendImpl::OnEvent(Stats::Counters an_event) {
  stats_.OnEvent(an_event);
}

void BackendImpl::OnStatsTimer() {
  stats_.OnEvent(Stats::TIMER);
  int64 current = stats_.GetCounter(Stats::OPEN_ENTRIES);
  int64 time = stats_.GetCounter(Stats::TIMER);

  current = current * (time - 1) + num_refs_;
  current /= time;
  stats_.SetCounter(Stats::OPEN_ENTRIES, current);
  stats_.SetCounter(Stats::MAX_ENTRIES, max_refs_);

  static bool first_time = true;
  if (!data_)
    first_time = false;
  if (first_time) {
    first_time = false;
    int experiment = data_->header.experiment;
    std::wstring entries(StringPrintf(L"DiskCache.Entries_%d", experiment));
    std::wstring size(StringPrintf(L"DiskCache.Size_%d", experiment));
    std::wstring max_size(StringPrintf(L"DiskCache.MaxSize_%d", experiment));
    UMA_HISTOGRAM_COUNTS(entries.c_str(), data_->header.num_entries);
    UMA_HISTOGRAM_COUNTS(size.c_str(), data_->header.num_bytes / (1024 * 1024));
    UMA_HISTOGRAM_COUNTS(max_size.c_str(), max_size_ / (1024 * 1024));
  }
}

void BackendImpl::IncrementIoCount() {
  num_pending_io_++;
}

void BackendImpl::DecrementIoCount() {
  num_pending_io_--;
}

void BackendImpl::SetUnitTestMode() {
  unit_test_ = true;
}

void BackendImpl::SetUpgradeMode() {
  read_only_ = true;
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
  return true;
}

void BackendImpl::AdjustMaxCacheSize(int table_len) {
  if (max_size_)
    return;

  // The user is not setting the size, let's figure it out.
  int64 available = base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (available < 0) {
    max_size_ = kDefaultCacheSize;
    return;
  }

  // Attempt to use 1% of the disk available for this user.
  available /= 100;

  if (available < kDefaultCacheSize)
    max_size_ = kDefaultCacheSize;
  else if (available > kint32max)
    max_size_ = kint32max;
  else
    max_size_ = static_cast<int32>(available);

  // Let's not use more than the default size while we tune-up the performance
  // of bigger caches. TODO(rvargas): remove this limit.
  // If we are creating the file, use 1 as the multiplier so the table size is
  // the same for everybody.
  int multiplier = table_len ? data_->header.experiment : 1;
  DCHECK(multiplier > 0 && multiplier < 5);
  max_size_ = kDefaultCacheSize * multiplier;

  if (!table_len)
    return;

  // If we already have a table, adjust the size to it.
  // NOTE: Disabled for the experiment.
  // int current_max_size = MaxStorageSizeForTable(table_len);
  // if (max_size_ > current_max_size)
  //   max_size_= current_max_size;
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
  data_->header.crash = 0;
  index_ = NULL;
  data_ = NULL;
  block_files_.CloseFiles();
  rankings_.Reset();
  init_ = false;
  restarted_ = true;

  // TODO(rvargas): remove this line at the end of this experiment.
  max_size_ = 0;
}

int BackendImpl::NewEntry(Addr address, EntryImpl** entry, bool* dirty) {
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

  cache_entry.swap(entry);
  return 0;
}

EntryImpl* BackendImpl::MatchEntry(const std::string& key, uint32 hash,
                                   bool find_parent) {
  Addr address(data_->table[hash & mask_]);
  EntryImpl* cache_entry = NULL;
  EntryImpl* parent_entry = NULL;
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
    int error = NewEntry(address, &cache_entry, &dirty);

    if (error || dirty) {
      // This entry is dirty on disk (it was not properly closed): we cannot
      // trust it.
      Addr child(0);
      if (!error)
        child.set_value(cache_entry->GetNextAddress());

      if (parent_entry) {
        parent_entry->SetNextAddress(child);
        parent_entry->Release();
        parent_entry = NULL;
      } else {
        data_->table[hash & mask_] = child.value();
      }

      if (!error) {
        // It is important to call DestroyInvalidEntry after removing this
        // entry from the table.
        DestroyInvalidEntry(address, cache_entry);
        cache_entry->Release();
        cache_entry = NULL;
      } else {
        Trace("NewEntry failed on MatchEntry 0x%x", address.value());
      }

      // Restart the search.
      address.set_value(data_->table[hash & mask_]);
      continue;
    }

    if (cache_entry->IsSameEntry(key, hash)) {
      cache_entry = EntryImpl::Update(cache_entry);
      found = true;
      break;
    }
    cache_entry = EntryImpl::Update(cache_entry);
    if (parent_entry)
      parent_entry->Release();
    parent_entry = cache_entry;
    cache_entry = NULL;
    if (!parent_entry)
      break;

    address.set_value(parent_entry->GetNextAddress());
  }

  if (parent_entry && (!find_parent || !found)) {
    parent_entry->Release();
    parent_entry = NULL;
  }

  if (cache_entry && (find_parent || !found)) {
    cache_entry->Release();
    cache_entry = NULL;
  }

  return find_parent ? parent_entry : cache_entry;
}

// This is the actual implementation for OpenNextEntry and OpenPrevEntry.
bool BackendImpl::OpenFollowingEntry(bool forward, void** iter,
                                     Entry** next_entry) {
  if (disabled_)
    return false;

  Rankings::ScopedRankingsBlock rankings(&rankings_,
      reinterpret_cast<CacheRankingsBlock*>(*iter));
  CacheRankingsBlock* next_block = forward ?
      rankings_.GetNext(rankings.get(), Rankings::NO_USE) :
      rankings_.GetPrev(rankings.get(), Rankings::NO_USE);
  Rankings::ScopedRankingsBlock next(&rankings_, next_block);
  *next_entry = NULL;
  *iter = NULL;
  if (!next.get())
    return false;

  scoped_refptr<EntryImpl> entry;
  if (next->Data()->pointer) {
    entry = reinterpret_cast<EntryImpl*>(next->Data()->pointer);
  } else {
    bool dirty;
    EntryImpl* temp = NULL;
    if (NewEntry(Addr(next->Data()->contents), &temp, &dirty))
      return false;
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

      return false;
    }

    entry.swap(&temp);
    temp = EntryImpl::Update(temp);  // Update returns an adref'd entry.
    entry.swap(&temp);
    if (!entry.get())
      return false;
  }

  entry.swap(reinterpret_cast<EntryImpl**>(next_entry));
  *iter = next.release();
  return true;
}

void BackendImpl::DestroyInvalidEntry(Addr address, EntryImpl* entry) {
  LOG(WARNING) << "Destroying invalid entry.";
  Trace("Destroying invalid entry 0x%p", entry);

  entry->SetPointerForInvalidEntry(GetCurrentEntryId());

  eviction_.OnDoomEntry(entry);
  entry->InternalDoom();

  data_->header.num_entries--;
  DCHECK(data_->header.num_entries >= 0);
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

void BackendImpl::LogStats() {
  StatsItems stats;
  GetStats(&stats);

  for (size_t index = 0; index < stats.size(); index++) {
    LOG(INFO) << stats[index].first << ": " << stats[index].second;
  }
}

bool BackendImpl::CheckIndex() {
  if (!data_) {
    LOG(ERROR) << "Unable to map Index file";
    return false;
  }

  size_t current_size = index_->GetLength();
  if (current_size < sizeof(Index)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  if (kIndexMagic != data_->header.magic ||
      kCurrentVersion != data_->header.version) {
    LOG(ERROR) << "Invalid file version or magic";
    return false;
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

  // TODO(rvargas): remove this. For some reason, we are receiving crashes with
  // mask_ being bigger than the actual table length. (bug 7217).
  if (mask_ > 0xffff) {
    LOG(ERROR) << "Invalid cache mask";
    ReportError(ERR_INVALID_MASK);
    return false;
  }

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
