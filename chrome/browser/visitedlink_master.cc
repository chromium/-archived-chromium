// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visitedlink_master.h"

#if defined(OS_WIN)
#include <windows.h>
#include <io.h>
#include <shlobj.h>
#endif  // defined(OS_WIN)
#include <stdio.h>

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stack_container.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"

#if defined(OS_WIN)
#include "chrome/common/win_util.h"
#endif

using file_util::ScopedFILE;
using file_util::OpenFile;
using file_util::TruncateFile;

const int32 VisitedLinkMaster::kFileHeaderSignatureOffset = 0;
const int32 VisitedLinkMaster::kFileHeaderVersionOffset = 4;
const int32 VisitedLinkMaster::kFileHeaderLengthOffset = 8;
const int32 VisitedLinkMaster::kFileHeaderUsedOffset = 12;
const int32 VisitedLinkMaster::kFileHeaderSaltOffset = 16;

const int32 VisitedLinkMaster::kFileCurrentVersion = 2;

// the signature at the beginning of the URL table = "VLnk" (visited links)
const int32 VisitedLinkMaster::kFileSignature = 0x6b6e4c56;
const size_t VisitedLinkMaster::kFileHeaderSize =
    kFileHeaderSaltOffset + LINK_SALT_LENGTH;

// This value should also be the same as the smallest size in the lookup
// table in NewTableSizeForCount (prime number).
const unsigned VisitedLinkMaster::kDefaultTableSize = 16381;

const size_t VisitedLinkMaster::kBigDeleteThreshold = 64;

namespace {

// Fills the given salt structure with some quasi-random values
// It is not necessary to generate a cryptographically strong random string,
// only that it be reasonably different for different users.
void GenerateSalt(uint8 salt[LINK_SALT_LENGTH]) {
  DCHECK_EQ(LINK_SALT_LENGTH, 8) << "This code assumes the length of the salt";
  uint64 randval = base::RandUint64();
  memcpy(salt, &randval, 8);
}

// AsyncWriter ----------------------------------------------------------------

// This task executes on a background thread and executes a write. This
// prevents us from blocking the UI thread doing I/O.
class AsyncWriter : public Task {
 public:
  AsyncWriter(FILE* file, int32 offset, const void* data, size_t data_len)
      : file_(file),
        offset_(offset) {
    data_->resize(data_len);
    memcpy(&*data_->begin(), data, data_len);
  }

  virtual void Run() {
    WriteToFile(file_, offset_,
                &*data_->begin(), static_cast<int32>(data_->size()));
  }

  // Exposed as a static so it can be called directly from the Master to
  // reduce the number of platform-specific I/O sites we have. Returns true if
  // the write was complete.
  static bool WriteToFile(FILE* file,
                          off_t offset,
                          const void* data,
                          size_t data_len) {
    if (fseek(file, offset, SEEK_SET) != 0)
      return false;  // Don't write to an invalid part of the file.

    size_t num_written = fwrite(data, 1, data_len, file);
    return num_written == data_len;
  }

 private:
  // The data to write and where to write it.
  FILE* file_;
  int32 offset_;  // Offset from the beginning of the file.

  // Most writes are just a single fingerprint, so we reserve that much in this
  // object to avoid mallocs in that case.
  StackVector<char, sizeof(VisitedLinkCommon::Fingerprint)> data_;

  DISALLOW_EVIL_CONSTRUCTORS(AsyncWriter);
};

// Used to asynchronously set the end of the file. This must be done on the
// same thread as the writing to keep things synchronized.
class AsyncSetEndOfFile : public Task {
 public:
  explicit AsyncSetEndOfFile(FILE* file) : file_(file) {}

  virtual void Run() {
    TruncateFile(file_);
  }

 private:
  FILE* file_;
  DISALLOW_EVIL_CONSTRUCTORS(AsyncSetEndOfFile);
};

// Used to asynchronously close a file. This must be done on the same thread as
// the writing to keep things synchronized.
class AsyncCloseHandle : public Task {
 public:
  explicit AsyncCloseHandle(FILE* file) : file_(file) {}

  virtual void Run() {
    fclose(file_);
  }

 private:
  FILE* file_;
  DISALLOW_EVIL_CONSTRUCTORS(AsyncCloseHandle);
};

}  // namespace

// TableBuilder ---------------------------------------------------------------

// How rebuilding from history works
// ---------------------------------
//
// We mark that we're rebuilding from history by setting the table_builder_
// member in VisitedLinkMaster to the TableBuilder we create. This builder
// will be called on the history thread by the history system for every URL
// in the database.
//
// The builder will store the fingerprints for those URLs, and then marshalls
// back to the main thread where the VisitedLinkMaster will be notified. The
// master then replaces its table with a new table containing the computed
// fingerprints.
//
// The builder must remain active while the history system is using it.
// Sometimes, the master will be deleted before the rebuild is complete, in
// which case it notifies the builder via DisownMaster(). The builder will
// delete itself once rebuilding is complete, and not execute any callback.
class VisitedLinkMaster::TableBuilder : public HistoryService::URLEnumerator,
                                        public base::RefCounted<TableBuilder> {
 public:
  TableBuilder(VisitedLinkMaster* master,
               const uint8 salt[LINK_SALT_LENGTH]);

  // Called on the main thread when the master is being destroyed. This will
  // prevent a crash when the query completes and the master is no longer
  // around. We can not actually do anything but mark this fact, since the
  // table will be being rebuilt simultaneously on the other thread.
  void DisownMaster();

  // HistoryService::URLEnumerator
  virtual void OnURL(const GURL& url);
  virtual void OnComplete(bool succeed);

 private:
  // OnComplete mashals to this function on the main thread to do the
  // notification.
  void OnCompleteMainThread();

  // Owner of this object. MAY ONLY BE ACCESSED ON THE MAIN THREAD!
  VisitedLinkMaster* master_;

  // The thread the visited link master is on where we will notify it.
  MessageLoop* main_message_loop_;

  // Indicates whether the operation has failed or not.
  bool success_;

  // Salt for this new table.
  uint8 salt_[LINK_SALT_LENGTH];

  // Stores the fingerprints we computed on the background thread.
  std::vector<VisitedLinkMaster::Fingerprint> fingerprints_;
};

// VisitedLinkMaster ----------------------------------------------------------

VisitedLinkMaster::VisitedLinkMaster(base::Thread* file_thread,
                                     PostNewTableEvent* poster,
                                     Profile* profile) {
  InitMembers(file_thread, poster, profile);
}

VisitedLinkMaster::VisitedLinkMaster(base::Thread* file_thread,
                                     PostNewTableEvent* poster,
                                     HistoryService* history_service,
                                     bool suppress_rebuild,
                                     const FilePath& filename,
                                     int32 default_table_size) {
  InitMembers(file_thread, poster, NULL);

  database_name_override_ = filename;
  table_size_override_ = default_table_size;
  history_service_override_ = history_service;
  suppress_rebuild_ = suppress_rebuild;
}

VisitedLinkMaster::~VisitedLinkMaster() {
  if (table_builder_.get()) {
    // Prevent the table builder from calling us back now that we're being
    // destroyed. Note that we DON'T delete the object, since the history
    // system is still writing into it. When that is complete, the table
    // builder will destroy itself when it finds we are gone.
    table_builder_->DisownMaster();
  }
  FreeURLTable();
}

void VisitedLinkMaster::InitMembers(base::Thread* file_thread,
                                    PostNewTableEvent* poster,
                                    Profile* profile) {
  if (file_thread)
    file_thread_ = file_thread->message_loop();
  else
    file_thread_ = NULL;

  post_new_table_event_ = poster;
  file_ = NULL;
  shared_memory_ = NULL;
  shared_memory_serial_ = 0;
  used_items_ = 0;
  table_size_override_ = 0;
  history_service_override_ = NULL;
  suppress_rebuild_ = false;
  profile_ = profile;

#ifndef NDEBUG
  posted_asynchronous_operation_ = false;
#endif
}

// The shared memory name should be unique on the system and also needs to
// change when we create a new table. The scheme we use includes the process
// ID, an increasing serial number, and the profile ID.
std::wstring VisitedLinkMaster::GetSharedMemoryName() const {
  // When unit testing, there's no profile, so use an empty ID string.
  std::wstring profile_id;
  if (profile_)
    profile_id = profile_->GetID().c_str();

  return StringPrintf(L"GVisitedLinks_%lu_%lu_%ls",
                      base::GetCurrentProcId(), shared_memory_serial_,
                      profile_id.c_str());
}

bool VisitedLinkMaster::Init() {
  if (!InitFromFile())
    return InitFromScratch(suppress_rebuild_);
  return true;
}

bool VisitedLinkMaster::ShareToProcess(base::ProcessHandle process,
                                       base::SharedMemoryHandle *new_handle) {
  if (shared_memory_)
    return shared_memory_->ShareToProcess(process, new_handle);

  NOTREACHED();
  return false;
}

base::SharedMemoryHandle VisitedLinkMaster::GetSharedMemoryHandle() {
  return shared_memory_->handle();
}

VisitedLinkMaster::Hash VisitedLinkMaster::TryToAddURL(const GURL& url) {
  // Extra check that we are not off the record. This should not happen.
  if (profile_ && profile_->IsOffTheRecord()) {
    NOTREACHED();
    return null_hash_;
  }

  if (!url.is_valid())
    return null_hash_;  // Don't add invalid URLs.

  Fingerprint fingerprint = ComputeURLFingerprint(url.spec().data(),
                                                  url.spec().size(),
                                                  salt_);
  if (table_builder_) {
    // If we have a pending delete for this fingerprint, cancel it.
    std::set<Fingerprint>::iterator found =
        deleted_since_rebuild_.find(fingerprint);
    if (found != deleted_since_rebuild_.end())
        deleted_since_rebuild_.erase(found);

    // A rebuild is in progress, save this addition in the temporary list so
    // it can be added once rebuild is complete.
    added_since_rebuild_.insert(fingerprint);
  }

  // If the table is "full", we don't add URLs and just drop them on the floor.
  // This can happen if we get thousands of new URLs and something causes
  // the table resizing to fail. This check prevents a hang in that case. Note
  // that this is *not* the resize limit, this is just a sanity check.
  if (used_items_ / 8 > table_length_ / 10)
    return null_hash_;  // Table is more than 80% full.

  return AddFingerprint(fingerprint);
}

void VisitedLinkMaster::AddURL(const GURL& url) {
  Hash index = TryToAddURL(url);
  if (!table_builder_ && index != null_hash_) {
    // Not rebuilding, so we want to keep the file on disk up-to-date.
    WriteUsedItemCountToFile();
    WriteHashRangeToFile(index, index);
    ResizeTableIfNecessary();
  }
}

void VisitedLinkMaster::AddURLs(const std::vector<GURL>& url) {
  for (std::vector<GURL>::const_iterator i = url.begin();
       i != url.end(); ++i) {
    Hash index = TryToAddURL(*i);
    if (!table_builder_ && index != null_hash_)
      ResizeTableIfNecessary();
  }

  // Keeps the file on disk up-to-date.
  if (!table_builder_)
    WriteFullTable();
}

void VisitedLinkMaster::DeleteAllURLs() {
  // Any pending modifications are invalid.
  added_since_rebuild_.clear();
  deleted_since_rebuild_.clear();

  // Clear the hash table.
  used_items_ = 0;
  memset(hash_table_, 0, this->table_length_ * sizeof(Fingerprint));

  // Resize it if it is now too empty. Resize may write the new table out for
  // us, otherwise, schedule writing the new table to disk ourselves.
  if (!ResizeTableIfNecessary())
    WriteFullTable();
}

void VisitedLinkMaster::DeleteURLs(const std::set<GURL>& urls) {
  typedef std::set<GURL>::const_iterator SetIterator;

  if (urls.empty())
    return;

  if (table_builder_) {
    // A rebuild is in progress, save this deletion in the temporary list so
    // it can be added once rebuild is complete.
    for (SetIterator i = urls.begin(); i != urls.end(); ++i) {
      if (!i->is_valid())
        continue;

      Fingerprint fingerprint =
          ComputeURLFingerprint(i->spec().data(), i->spec().size(), salt_);
      deleted_since_rebuild_.insert(fingerprint);

      // If the URL was just added and now we're deleting it, it may be in the
      // list of things added since the last rebuild. Delete it from that list.
      std::set<Fingerprint>::iterator found =
          added_since_rebuild_.find(fingerprint);
      if (found != added_since_rebuild_.end())
        added_since_rebuild_.erase(found);

      // Delete the URLs from the in-memory table, but don't bother writing
      // to disk since it will be replaced soon.
      DeleteFingerprint(fingerprint, false);
    }
    return;
  }

  // Compute the deleted URLs' fingerprints and delete them
  std::set<Fingerprint> deleted_fingerprints;
  for (SetIterator i = urls.begin(); i != urls.end(); ++i) {
    if (!i->is_valid())
      continue;
    deleted_fingerprints.insert(
        ComputeURLFingerprint(i->spec().data(), i->spec().size(), salt_));
  }
  DeleteFingerprintsFromCurrentTable(deleted_fingerprints);
}

// See VisitedLinkCommon::IsVisited which should be in sync with this algorithm
VisitedLinkMaster::Hash VisitedLinkMaster::AddFingerprint(
    Fingerprint fingerprint) {
  if (!hash_table_ || table_length_ == 0) {
    NOTREACHED();  // Not initialized.
    return null_hash_;
  }

  Hash cur_hash = HashFingerprint(fingerprint);
  Hash first_hash = cur_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == fingerprint)
      return null_hash_;  // This fingerprint is already in there, do nothing.

    if (cur_fingerprint == null_fingerprint_) {
      // End of probe sequence found, insert here.
      hash_table_[cur_hash] = fingerprint;
      used_items_++;
      return cur_hash;
    }

    // Advance in the probe sequence.
    cur_hash = IncrementHash(cur_hash);
    if (cur_hash == first_hash) {
      // This means that we've wrapped around and are about to go into an
      // infinite loop. Something was wrong with the hashtable resizing
      // logic, so stop here.
      NOTREACHED();
      return null_hash_;
    }
  }
}

void VisitedLinkMaster::DeleteFingerprintsFromCurrentTable(
    const std::set<Fingerprint>& fingerprints) {
  bool bulk_write = (fingerprints.size() > kBigDeleteThreshold);

  // Delete the URLs from the table.
  for (std::set<Fingerprint>::const_iterator i = fingerprints.begin();
       i != fingerprints.end(); ++i)
    DeleteFingerprint(*i, !bulk_write);

  // These deleted fingerprints may make us shrink the table.
  if (ResizeTableIfNecessary())
    return;  // The resize function wrote the new table to disk for us.

  // Nobody wrote this out for us, write the full file to disk.
  if (bulk_write)
    WriteFullTable();
}

bool VisitedLinkMaster::DeleteFingerprint(Fingerprint fingerprint,
                                          bool update_file) {
  if (!hash_table_ || table_length_ == 0) {
    NOTREACHED();  // Not initialized.
    return false;
  }
  if (!IsVisited(fingerprint))
    return false;  // Not in the database to delete.

  // First update the header used count.
  used_items_--;
  if (update_file)
    WriteUsedItemCountToFile();

  Hash deleted_hash = HashFingerprint(fingerprint);

  // Find the range of "stuff" in the hash table that is adjacent to this
  // fingerprint. These are things that could be affected by the change in
  // the hash table. Since we use linear probing, anything after the deleted
  // item up until an empty item could be affected.
  Hash end_range = deleted_hash;
  while (true) {
    Hash next_hash = IncrementHash(end_range);
    if (next_hash == deleted_hash)
      break;  // We wrapped around and the whole table is full.
    if (!hash_table_[next_hash])
      break;  // Found the last spot.
    end_range = next_hash;
  }

  // We could get all fancy and move the affected fingerprints around, but
  // instead we just remove them all and re-add them (minus our deleted one).
  // This will mean there's a small window of time where the affected links
  // won't be marked visited.
  StackVector<Fingerprint, 32> shuffled_fingerprints;
  Hash stop_loop = IncrementHash(end_range);  // The end range is inclusive.
  for (Hash i = deleted_hash; i != stop_loop; i = IncrementHash(i)) {
    if (hash_table_[i] != fingerprint) {
      // Don't save the one we're deleting!
      shuffled_fingerprints->push_back(hash_table_[i]);

      // This will balance the increment of this value in AddFingerprint below
      // so there is no net change.
      used_items_--;
    }
    hash_table_[i] = null_fingerprint_;
  }

  if (!shuffled_fingerprints->empty()) {
    // Need to add the new items back.
    for (size_t i = 0; i < shuffled_fingerprints->size(); i++)
      AddFingerprint(shuffled_fingerprints[i]);
  }

  // Write the affected range to disk [deleted_hash, end_range].
  if (update_file)
    WriteHashRangeToFile(deleted_hash, end_range);

  return true;
}

bool VisitedLinkMaster::WriteFullTable() {
  // This function can get called when the file is open, for example, when we
  // resize the table. We must handle this case and not try to reopen the file,
  // since there may be write operations pending on the file I/O thread.
  //
  // Note that once we start writing, we do not delete on error. This means
  // there can be a partial file, but the short file will be detected next time
  // we start, and will be replaced.
  //
  // This might possibly get corrupted if we crash in the middle of writing.
  // We should pick up the most common types of these failures when we notice
  // that the file size is different when we load it back in, and then we will
  // regenerate the table.
  ScopedFILE file_closer;  // Valid only when not open already.
  FILE* file;                         // Always valid.
  if (file_) {
    file = file_;
  } else {
    FilePath filename;
    GetDatabaseFileName(&filename);
    file_closer.reset(OpenFile(filename, "wb+"));
    if (!file_closer.get()) {
      DLOG(ERROR) << "Failed to open file " << filename.value();
      return false;
    }
    file = file_closer.get();
  }

  // Write the new header.
  int32 header[4];
  header[0] = kFileSignature;
  header[1] = kFileCurrentVersion;
  header[2] = table_length_;
  header[3] = used_items_;
  WriteToFile(file, 0, header, sizeof(header));
  WriteToFile(file, sizeof(header), salt_, LINK_SALT_LENGTH);

  // Write the hash data.
  WriteToFile(file, kFileHeaderSize,
              hash_table_, table_length_ * sizeof(Fingerprint));

  // The hash table may have shrunk, so make sure this is the end.
  if (file_thread_) {
    AsyncSetEndOfFile* setter = new AsyncSetEndOfFile(file);
    file_thread_->PostTask(FROM_HERE, setter);
  } else {
    TruncateFile(file);
  }

  // Keep the file open so we can dynamically write changes to it. When the
  // file was already open, the file_closer is NULL, and file_ is already good.
  if (file_closer.get())
    file_ = file_closer.release();
  return true;
}

bool VisitedLinkMaster::InitFromFile() {
  DCHECK(file_ == NULL);

  FilePath filename;
  GetDatabaseFileName(&filename);
  ScopedFILE file_closer(OpenFile(filename, "rb+"));
  if (!file_closer.get())
    return false;

  int32 num_entries, used_count;
  if (!ReadFileHeader(file_closer.get(), &num_entries, &used_count, salt_))
    return false;  // Header isn't valid.

  // Allocate and read the table.
  if (!CreateURLTable(num_entries, false))
    return false;
  if (!ReadFromFile(file_closer.get(), kFileHeaderSize,
                    hash_table_, num_entries * sizeof(Fingerprint))) {
    FreeURLTable();
    return false;
  }
  used_items_ = used_count;

  file_ = file_closer.release();
  return true;
}

bool VisitedLinkMaster::InitFromScratch(bool suppress_rebuild) {
  int32 table_size = kDefaultTableSize;
  if (table_size_override_)
    table_size = table_size_override_;

  // The salt must be generated before the table so that it can be copied to
  // the shared memory.
  GenerateSalt(salt_);
  if (!CreateURLTable(table_size, true))
    return false;

  if (suppress_rebuild) {
    // When we disallow rebuilds (normally just unit tests), just use the
    // current empty table.
    return WriteFullTable();
  }

  // This will build the table from history. On the first run, history will
  // be empty, so this will be correct. This will also write the new table
  // to disk. We don't want to save explicitly here, since the rebuild may
  // not complete, leaving us with an empty but valid visited link database.
  // In the future, we won't know we need to try rebuilding again.
  return RebuildTableFromHistory();
}

bool VisitedLinkMaster::ReadFileHeader(FILE* file,
                                       int32* num_entries,
                                       int32* used_count,
                                       uint8 salt[LINK_SALT_LENGTH]) {
  // Get file size.
  // Note that there is no need to seek back to the original location in the
  // file since ReadFromFile() [which is the next call accessing the file]
  // seeks before reading.
  if (fseek(file, 0, SEEK_END) == -1)
    return false;
  size_t file_size = ftell(file);

  if (file_size <= kFileHeaderSize)
    return false;

  uint8 header[kFileHeaderSize];
  if (!ReadFromFile(file, 0, &header, kFileHeaderSize))
    return false;

  // Verify the signature.
  int32 signature;
  memcpy(&signature, &header[kFileHeaderSignatureOffset], sizeof(signature));
  if (signature != kFileSignature)
    return false;

  // Verify the version is up-to-date. As with other read errors, a version
  // mistmatch will trigger a rebuild of the database from history, which will
  // have the effect of migrating the database.
  int32 version;
  memcpy(&version, &header[kFileHeaderVersionOffset], sizeof(version));
  if (version != kFileCurrentVersion)
    return false;  // Bad version.

  // Read the table size and make sure it matches the file size.
  memcpy(num_entries, &header[kFileHeaderLengthOffset], sizeof(*num_entries));
  if (*num_entries * sizeof(Fingerprint) + kFileHeaderSize != file_size)
    return false;  // Bad size.

  // Read the used item count.
  memcpy(used_count, &header[kFileHeaderUsedOffset], sizeof(*used_count));
  if (*used_count > *num_entries)
    return false;  // Bad used item count;

  // Read the salt.
  memcpy(salt, &header[kFileHeaderSaltOffset], LINK_SALT_LENGTH);

  // This file looks OK from the header's perspective.
  return true;
}

bool VisitedLinkMaster::GetDatabaseFileName(FilePath* filename) {
  if (!database_name_override_.empty()) {
    // use this filename, the directory must exist
    *filename = database_name_override_;
    return true;
  }

  if (!profile_ || profile_->GetPath().empty())
    return false;

  FilePath profile_dir = profile_->GetPath();
  *filename = profile_dir.Append(FILE_PATH_LITERAL("Visited Links"));
  return true;
}

// Initializes the shared memory structure. The salt should already be filled
// in so that it can be written to the shared memory
bool VisitedLinkMaster::CreateURLTable(int32 num_entries, bool init_to_empty) {
  // The table is the size of the table followed by the entries.
  int32 alloc_size = num_entries * sizeof(Fingerprint) + sizeof(SharedHeader);

  // Create the shared memory object.
  shared_memory_ = new base::SharedMemory();
  if (!shared_memory_)
    return false;

  if (!shared_memory_->Create(GetSharedMemoryName().c_str(),
      false, false, alloc_size))
    return false;

  // Map into our process.
  if (!shared_memory_->Map(alloc_size)) {
    delete shared_memory_;
    shared_memory_ = NULL;
    return false;
  }

  if (init_to_empty) {
    memset(shared_memory_->memory(), 0, alloc_size);
    used_items_ = 0;
  }
  table_length_ = num_entries;

  // Save the header for other processes to read.
  SharedHeader* header = static_cast<SharedHeader*>(shared_memory_->memory());
  header->length = table_length_;
  memcpy(header->salt, salt_, LINK_SALT_LENGTH);

  // Our table pointer is just the data immediately following the size.
  hash_table_ = reinterpret_cast<Fingerprint*>(
      static_cast<char*>(shared_memory_->memory()) + sizeof(SharedHeader));

#ifndef NDEBUG
  DebugValidate();
#endif

  return true;
}

bool VisitedLinkMaster::BeginReplaceURLTable(int32 num_entries) {
  base::SharedMemory *old_shared_memory = shared_memory_;
  Fingerprint* old_hash_table = hash_table_;
  int32 old_table_length = table_length_;
  if (!CreateURLTable(num_entries, true)) {
    // Try to put back the old state.
    shared_memory_ = old_shared_memory;
    hash_table_ = old_hash_table;
    table_length_ = old_table_length;
    return false;
  }
  return true;
}

void VisitedLinkMaster::FreeURLTable() {
  if (shared_memory_) {
    delete shared_memory_;
    shared_memory_ = NULL;
  }
  if (file_) {
    if (file_thread_) {
      AsyncCloseHandle* closer = new AsyncCloseHandle(file_);
      file_thread_->PostTask(FROM_HERE, closer);
    } else {
      fclose(file_);
    }
  }
}

bool VisitedLinkMaster::ResizeTableIfNecessary() {
  DCHECK(table_length_ > 0) << "Must have a table";

  // Load limits for good performance/space. We are pretty conservative about
  // keeping the table not very full. This is because we use linear probing
  // which increases the likelihood of clumps of entries which will reduce
  // performance.
  const float max_table_load = 0.5f;  // Grow when we're > this full.
  const float min_table_load = 0.2f;  // Shrink when we're < this full.

  float load = ComputeTableLoad();
  if (load < max_table_load &&
      (table_length_ <= static_cast<float>(kDefaultTableSize) ||
       load > min_table_load))
    return false;

  // Table needs to grow or shrink.
  int new_size = NewTableSizeForCount(used_items_);
  DCHECK(new_size > used_items_);
  DCHECK(load <= min_table_load || new_size > table_length_);
  ResizeTable(new_size);
  return true;
}

void VisitedLinkMaster::ResizeTable(int32 new_size) {
  DCHECK(shared_memory_ && shared_memory_->memory() && hash_table_);
  shared_memory_serial_++;

#ifndef NDEBUG
  DebugValidate();
#endif

  base::SharedMemory* old_shared_memory = shared_memory_;
  Fingerprint* old_hash_table = hash_table_;
  int32 old_table_length = table_length_;
  if (!BeginReplaceURLTable(new_size))
    return;

  // Now we have two tables, our local copy which is the old one, and the new
  // one loaded into this object where we need to copy the data.
  for (int32 i = 0; i < old_table_length; i++) {
    Fingerprint cur = old_hash_table[i];
    if (cur)
      AddFingerprint(cur);
  }

  // On error unmapping, just forget about it since we can't do anything
  // else to release it.
  delete old_shared_memory;

  // Send an update notification to all child processes so they read the new
  // table.
  post_new_table_event_(shared_memory_);

#ifndef NDEBUG
  DebugValidate();
#endif

  // The new table needs to be written to disk.
  WriteFullTable();
}

uint32 VisitedLinkMaster::NewTableSizeForCount(int32 item_count) const {
  // These table sizes are selected to be the maximum prime number less than
  // a "convenient" multiple of 1K.
  static const int table_sizes[] = {
      16381,    // 16K  = 16384   <- don't shrink below this table size
                //                   (should be == default_table_size)
      32767,    // 32K  = 32768
      65521,    // 64K  = 65536
      130051,   // 128K = 131072
      262127,   // 256K = 262144
      524269,   // 512K = 524288
      1048549,  // 1M   = 1048576
      2097143,  // 2M   = 2097152
      4194301,  // 4M   = 4194304
      8388571,  // 8M   = 8388608
      16777199,  // 16M  = 16777216
      33554347};  // 32M  = 33554432

  // Try to leave the table 33% full.
  int desired = item_count * 3;

  // Find the closest prime.
  for (size_t i = 0; i < arraysize(table_sizes); i ++) {
    if (table_sizes[i] > desired)
      return table_sizes[i];
  }

  // Growing very big, just approximate a "good" number, not growing as much
  // as normal.
  return item_count * 2 - 1;
}

// See the TableBuilder definition in the header file for how this works.
bool VisitedLinkMaster::RebuildTableFromHistory() {
  DCHECK(!table_builder_);
  if (table_builder_)
    return false;

  HistoryService* history_service = history_service_override_;
  if (!history_service && profile_) {
    history_service = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  if (!history_service) {
    DLOG(WARNING) << "Attempted to rebuild visited link table, but couldn't "
                     "obtain a HistoryService.";
    return false;
  }

  // TODO(brettw) make sure we have reasonable salt!
  table_builder_ = new TableBuilder(this, salt_);

  // Make sure the table builder stays live during the call, even if the
  // master is deleted. This is balanced in TableBuilder::OnCompleteMainThread.
  table_builder_->AddRef();
  history_service->IterateURLs(table_builder_);
  return true;
}

// See the TableBuilder definition in the header file for how this works.
void VisitedLinkMaster::OnTableRebuildComplete(
    bool success,
    const std::vector<Fingerprint>& fingerprints) {
  if (success) {
    // Replace the old table with a new blank one.
    shared_memory_serial_++;

    // We are responsible for freeing it AFTER it has been replaced if
    // replacement succeeds.
    base::SharedMemory* old_shared_memory = shared_memory_;

    int new_table_size = NewTableSizeForCount(
        static_cast<int>(fingerprints.size()));
    if (BeginReplaceURLTable(new_table_size)) {
      // Free the old table.
      delete old_shared_memory;

      // Add the stored fingerprints to the hash table.
      for (size_t i = 0; i < fingerprints.size(); i++)
        AddFingerprint(fingerprints[i]);

      // Also add anything that was added while we were asynchronously
      // generating the new table.
      for (std::set<Fingerprint>::iterator i = added_since_rebuild_.begin();
           i != added_since_rebuild_.end(); ++i)
        AddFingerprint(*i);
      added_since_rebuild_.clear();

      // Now handle deletions.
      DeleteFingerprintsFromCurrentTable(deleted_since_rebuild_);
      deleted_since_rebuild_.clear();

      // Send an update notification to all child processes.
      post_new_table_event_(shared_memory_);

      WriteFullTable();
    }
  }
  table_builder_ = NULL;  // Will release our reference to the builder.

  // Notify the unit test that the rebuild is complete (will be NULL in prod.)
  if (rebuild_complete_task_.get()) {
    rebuild_complete_task_->Run();
    rebuild_complete_task_.reset(NULL);
  }
}

void VisitedLinkMaster::WriteToFile(FILE* file,
                                    off_t offset,
                                    void* data,
                                    int32 data_size) {
#ifndef NDEBUG
  posted_asynchronous_operation_ = true;
#endif

  if (file_thread_) {
    // Send the write to the other thread for execution to avoid blocking.
    AsyncWriter* writer = new AsyncWriter(file, offset, data, data_size);
    file_thread_->PostTask(FROM_HERE, writer);
  } else {
    // When there is no I/O thread, we are probably running in unit test mode,
    // just do the write synchronously.
    AsyncWriter::WriteToFile(file, offset, data, data_size);
  }
}

void VisitedLinkMaster::WriteUsedItemCountToFile() {
  WriteToFile(file_, kFileHeaderUsedOffset, &used_items_, sizeof(used_items_));
}

void VisitedLinkMaster::WriteHashRangeToFile(Hash first_hash, Hash last_hash) {
  if (last_hash < first_hash) {
    // Handle wraparound at 0. This first write is first_hash->EOF
    WriteToFile(file_, first_hash * sizeof(Fingerprint) + kFileHeaderSize,
                &hash_table_[first_hash],
                (table_length_ - first_hash + 1) * sizeof(Fingerprint));

    // Now do 0->last_lash.
    WriteToFile(file_, kFileHeaderSize, hash_table_,
                (last_hash + 1) * sizeof(Fingerprint));
  } else {
    // Normal case, just write the range.
    WriteToFile(file_, first_hash * sizeof(Fingerprint) + kFileHeaderSize,
                &hash_table_[first_hash],
                (last_hash - first_hash + 1) * sizeof(Fingerprint));
  }
}

bool VisitedLinkMaster::ReadFromFile(FILE* file,
                                     off_t offset,
                                     void* data,
                                     size_t data_size) {
#ifndef NDEBUG
  // Since this function is synchronous, we require that no asynchronous
  // operations could possibly be pending.
  DCHECK(!posted_asynchronous_operation_);
#endif

  fseek(file, offset, SEEK_SET);

  size_t num_read = fread(data, 1, data_size, file);
  return num_read == data_size;
}

// VisitedLinkTableBuilder ----------------------------------------------------

VisitedLinkMaster::TableBuilder::TableBuilder(
    VisitedLinkMaster* master,
    const uint8 salt[LINK_SALT_LENGTH])
    : master_(master),
      main_message_loop_(MessageLoop::current()),
      success_(true) {
  fingerprints_.reserve(4096);
  memcpy(salt_, salt, sizeof(salt));
}

// TODO(brettw): Do we want to try to cancel the request if this happens? It
// could delay shutdown if there are a lot of URLs.
void VisitedLinkMaster::TableBuilder::DisownMaster() {
  master_ = NULL;
}

void VisitedLinkMaster::TableBuilder::OnURL(const GURL& url) {
  if (!url.is_empty()) {
    fingerprints_.push_back(VisitedLinkMaster::ComputeURLFingerprint(
        url.spec().data(), url.spec().length(), salt_));
  }
}

void VisitedLinkMaster::TableBuilder::OnComplete(bool success) {
  success_ = success;
  DLOG_IF(WARNING, !success) << "Unable to rebuild visited links";

  // Marshal to the main thread to notify the VisitedLinkMaster that the
  // rebuild is complete.
  main_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &TableBuilder::OnCompleteMainThread));
}

void VisitedLinkMaster::TableBuilder::OnCompleteMainThread() {
  if (master_)
    master_->OnTableRebuildComplete(success_, fingerprints_);

  // WILL (generally) DELETE THIS! This balances the AddRef in
  // VisitedLinkMaster::RebuildTableFromHistory.
  Release();
}
