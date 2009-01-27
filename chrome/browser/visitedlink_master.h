// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITEDLINK_MASTER_H__
#define CHROME_BROWSER_VISITEDLINK_MASTER_H__

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#if defined(OS_WIN)
#include "chrome/browser/history/history.h"
#else
// TODO(port): remove scaffolding, use history.h for both POSIX and WIN.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif  // !defined(OS_WIN)
#include "chrome/common/visitedlink_common.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class GURL;
class Profile;

// Controls the link coloring database. The master controls all writing to the
// database as well as disk I/O. There should be only one master.
//
// This class will optionally defer writing operations to another thread. This
// means that after class destruction, the file may still be open since
// operations are pending on another thread.
class VisitedLinkMaster : public VisitedLinkCommon {
 public:
   typedef void (PostNewTableEvent)(base::SharedMemory*);

  // The |file_thread| may be NULL, in which case write operations will be
  // synchronous.
  VisitedLinkMaster(base::Thread* file_thread,
                    PostNewTableEvent* poster,
                    Profile* profile);

  // In unit test mode, we allow the caller to optionally specify the database
  // filename so that it can be run from a unit test. The directory where this
  // file resides must exist in this mode. You can also specify the default
  // table size to test table resizing. If this parameter is 0, we will use the
  // defaults.
  //
  // In the unit test mode, we also allow the caller to provide a history
  // service pointer (the history service can't be fetched from the browser
  // process when we're in unit test mode). This can be NULL to try to access
  // the main version, which will probably fail (which can be good for testing
  // this failure mode).
  //
  // When |suppress_rebuild| is set, we'll not attempt to load data from
  // history if the file can't be loaded. This should generally be set for
  // testing except when you want to test the rebuild process explicitly.
  VisitedLinkMaster(base::Thread* file_thread,
                    PostNewTableEvent* poster,
                    HistoryService* history_service,
                    bool suppress_rebuild,
                    const FilePath& filename,
                    int32 default_table_size);
  virtual ~VisitedLinkMaster();

  // Must be called immediately after object creation. Nothing else will work
  // until this is called. Returns true on success, false means that this
  // object won't work.
  bool Init();

  // Duplicates the handle to the shared memory to another process.
  // Returns true on success.
  bool ShareToProcess(base::ProcessHandle process,
                      base::SharedMemoryHandle *new_handle);

  // returns the name of the shared memory object that slaves can use to map
  // the data
  std::wstring GetSharedMemoryName() const;

  // Returns the handle to the shared memory
  base::SharedMemoryHandle GetSharedMemoryHandle();

  // Adds a URL to the table.
  void AddURL(const GURL& url);

  // Adds a set of URLs to the table.
  void AddURLs(const std::vector<GURL>& url);

  // Deletes the specified URLs from the table.
  void DeleteURLs(const std::set<GURL>& urls);

  // Clears the visited links table by deleting the file from disk. Used as
  // part of history clearing.
  void DeleteAllURLs();

#if defined(UNIT_TEST) || !defined(NDEBUG) || defined(PERF_TEST)
  // This is a debugging function that can be called to double-check internal
  // data structures. It will assert if the check fails.
  void DebugValidate();

  // Sets a task to execute when the next rebuild from history is complete.
  // This is used by unit tests to wait for the rebuild to complete before
  // they continue. The pointer will be owned by this object after the call.
  void set_rebuild_complete_task(Task* task) {
    DCHECK(!rebuild_complete_task_.get());
    rebuild_complete_task_.reset(task);
  }

  // returns the number of items in the table for testing verification
  int32 GetUsedCount() const {
    return used_items_;
  }

  // Call to cause the entire database file to be re-written from scratch
  // to disk. Used by the performance tester.
  bool RewriteFile() {
    return WriteFullTable();
  }
#endif

 private:
  FRIEND_TEST(VisitedLinkTest, Delete);
  FRIEND_TEST(VisitedLinkTest, BigDelete);

  // Object to rebuild the table on the history thread (see the .cc file).
  class TableBuilder;

  // Byte offsets of values in the header.
  static const int32 kFileHeaderSignatureOffset;
  static const int32 kFileHeaderVersionOffset;
  static const int32 kFileHeaderLengthOffset;
  static const int32 kFileHeaderUsedOffset;
  static const int32 kFileHeaderSaltOffset;

  // The signature at the beginning of a file.
  static const int32 kFileSignature;

  // version of the file format this module currently uses
  static const int32 kFileCurrentVersion;

  // Bytes in the file header, including the salt.
  static const size_t kFileHeaderSize;

  // When creating a fresh new table, we use this many entries.
  static const unsigned kDefaultTableSize;

  // When the user is deleting a boatload of URLs, we don't really want to do
  // individual writes for each of them. When the count exceeds this threshold,
  // we will write the whole table to disk at once instead of individual items.
  static const size_t kBigDeleteThreshold;

  // Backend for the constructors initializing the members.
  void InitMembers(base::Thread* file_thread,
                   PostNewTableEvent* poster,
                   Profile* profile);

  // If a rebuild is in progress, we save the URL in the temporary list.
  // Otherwise, we add this to the table. Returns the index of the
  // inserted fingerprint or null_hash_ on failure.
  Hash TryToAddURL(const GURL& url);

  // File I/O functions
  // ------------------

  // Writes the entire table to disk, returning true on success. It will leave
  // the table file open and the handle to it in file_
  bool WriteFullTable();

  // Try to load the table from the database file. If the file doesn't exist or
  // is corrupt, this will return failure.
  bool InitFromFile();

  // Reads the header of the link coloring database from disk. Assumes the
  // file pointer is at the beginning of the file and that there are no pending
  // asynchronous I/O operations.
  //
  // Returns true on success and places the size of the table in num_entries
  // and the number of nonzero fingerprints in used_count. This will fail if
  // the version of the file is not the current version of the database.
  bool ReadFileHeader(FILE* hfile, int32* num_entries, int32* used_count,
                      uint8 salt[LINK_SALT_LENGTH]);

  // Fills *filename with the name of the link database filename
  bool GetDatabaseFileName(FilePath* filename);

  // Wrapper around Window's WriteFile using asynchronous I/O. This will proxy
  // the write to a background thread.
  void WriteToFile(FILE* hfile, off_t offset, void* data, int32 data_size);

  // Helper function to schedule and asynchronous write of the used count to
  // disk (this is a common operation).
  void WriteUsedItemCountToFile();

  // Helper function to schedule an asynchronous write of the given range of
  // hash functions to disk. The range is inclusive on both ends. The range can
  // wrap around at 0 and this function will handle it.
  void WriteHashRangeToFile(Hash first_hash, Hash last_hash);

  // Synchronous read from the file. Assumes there are no pending asynchronous
  // I/O functions. Returns true if the entire buffer was successfully filled.
  bool ReadFromFile(FILE* hfile, off_t offset, void* data, size_t data_size);

  // General table handling
  // ----------------------

  // Called to add a fingerprint to the table. Returns the index of the
  // inserted fingerprint or null_hash_ if there was a duplicate and this item
  // was skippped.
  Hash AddFingerprint(Fingerprint fingerprint);

  // Deletes all fingerprints from the given vector from the current hash table
  // and syncs it to disk if there are changes. This does not update the
  // deleted_since_rebuild_ list, the caller must update this itself if there
  // is an update pending.
  void DeleteFingerprintsFromCurrentTable(
      const std::set<Fingerprint>& fingerprints);

  // Removes the indicated fingerprint from the table. If the update_file flag
  // is set, the changes will also be written to disk. Returns true if the
  // fingerprint was deleted, false if it was not in the table to delete.
  bool DeleteFingerprint(Fingerprint fingerprint, bool update_file);

  // Creates a new empty table, call if InitFromFile() fails. Normally, when
  // |suppress_rebuild| is false, the table will be rebuilt from history,
  // keeping us in sync. When |suppress_rebuild| is true, the new table will be
  // empty and we will not consult history. This is used when clearing the
  // database and for unit tests.
  bool InitFromScratch(bool suppress_rebuild);

  // Allocates the Fingerprint structure and length. When init_to_empty is set,
  // the table will be filled with 0s and used_items_ will be set to 0 as well.
  // If the flag is not set, these things are untouched and it is the
  // responsibility of the caller to fill them (like when we are reading from
  // a file).
  bool CreateURLTable(int32 num_entries, bool init_to_empty);

  // A wrapper for CreateURLTable, this will allocate a new table, initialized
  // to empty. The caller is responsible for saving the shared memory pointer
  // and handles before this call (they will be replaced with new ones) and
  // releasing them later. This is designed for callers that make a new table
  // and then copy values from the old table to the new one, then release the
  // old table.
  //
  // Returns true on success. On failure, the old table will be restored. The
  // caller should not attemp to release the pointer/handle in this case.
  bool BeginReplaceURLTable(int32 num_entries);

  // unallocates the Fingerprint table
  void FreeURLTable();

  // For growing the table. ResizeTableIfNecessary will check to see if the
  // table should be resized and calls ResizeTable if needed. Returns true if
  // we decided to resize the table.
  bool ResizeTableIfNecessary();

  // Resizes the table (growing or shrinking) as necessary to accomodate the
  // current count.
  void ResizeTable(int32 new_size);

  // Returns the desired table size for |item_count| URLs.
  uint32 NewTableSizeForCount(int32 item_count) const;

  // Computes the table load as fraction. For example, if 1/4 of the entries are
  // full, this value will be 0.25
  float ComputeTableLoad() const {
    return static_cast<float>(used_items_) / static_cast<float>(table_length_);
  }

  // Initializes a rebuild of the visited link database based on the browser
  // history. This will set table_builder_ while working, and there should not
  // already be a rebuild in place when called. See the definition for more
  // details on how this works.
  //
  // Returns true on success. Failure means we're not attempting to rebuild
  // the database because something failed.
  bool RebuildTableFromHistory();

  // Callback that the table rebuilder uses when the rebuild is complete.
  // |success| is true if the fingerprint generation succeeded, in which case
  // |fingerprints| will contain the computed fingerprints. On failure, there
  // will be no fingerprints.
  void OnTableRebuildComplete(bool success,
                              const std::vector<Fingerprint>& fingerprints);

  // Increases or decreases the given hash value by one, wrapping around as
  // necessary. Used for probing.
  inline Hash IncrementHash(Hash hash) {
    if (hash >= table_length_ - 1)
      return 0;  // Wrap around.
    return hash + 1;
  }
  inline Hash DecrementHash(Hash hash) {
    if (hash <= 0)
      return table_length_ - 1;  // Wrap around.
    return hash - 1;
  }

  PostNewTableEvent* post_new_table_event_;

#ifndef NDEBUG
  // Indicates whether any asynchronous operation has ever been completed.
  // We do some synchronous reads that require that no asynchronous operations
  // are pending, yet we don't track whether they have been completed. This
  // flag is a sanity check that these reads only happen before any
  // asynchronous writes have been fired.
  bool posted_asynchronous_operation_;
#endif

  // The thread where we do write operations from to avoid synchronous I/O on
  // the main thread. This may be NULL, which indicates synchronous I/O.
  MessageLoop* file_thread_;

  // Reference to the user profile that this object belongs to
  // (it knows the path to where the data is stored)
  Profile* profile_;

  // When non-NULL, indicates we are in database rebuild mode and points to
  // the class collecting fingerprint information from the history system.
  // The pointer is owned by this class, but it must remain valid while the
  // history query is running. We must only delete it when the query is done.
  scoped_refptr<TableBuilder> table_builder_;

  // Indicates URLs added and deleted since we started rebuilding the table.
  std::set<Fingerprint> added_since_rebuild_;
  std::set<Fingerprint> deleted_since_rebuild_;

  // TODO(brettw) Support deletion, we need to track whether anything was
  // deleted during the rebuild here. Then we should delete any of these
  // entries from the complete table later.
  //std::vector<Fingerprint> removed_since_rebuild_;

  // The currently open file with the table in it. This may be NULL if we're
  // rebuilding and haven't written a new version yet. Writing to the file may
  // be safely ignored in this case.
  FILE* file_;

  // Shared memory consists of a SharedHeader followed by the table.
  base::SharedMemory *shared_memory_;

  // When we generate new tables, we increment the serial number of the
  // shared memory object.
  int32 shared_memory_serial_;

  // Number of non-empty items in the table, used to compute fullness.
  int32 used_items_;

  // Testing values -----------------------------------------------------------
  //
  // The following fields exist for testing purposes. They are not used in
  // release builds. It'd be nice to eliminate them in release builds, but we
  // don't want to change the signature of the object between the unit test and
  // regular builds. Instead, we just have "default" values that these take
  // in release builds that give "regular" behavior.

  // Overridden database file name for testing
  FilePath database_name_override_;

  // When nonzero, overrides the table size for new databases for testing
  int32 table_size_override_;

  // When non-NULL, overrides the history service to use rather than as the
  // BrowserProcess. This is provided for unit tests.
  HistoryService* history_service_override_;

  // When non-NULL, indicates the task that should be run after the next
  // rebuild from history is complete.
  scoped_ptr<Task> rebuild_complete_task_;

  // Set to prevent us from attempting to rebuild the database from global
  // history if we have an error opening the file. This is used for testing,
  // will be false in production.
  bool suppress_rebuild_;

  DISALLOW_EVIL_CONSTRUCTORS(VisitedLinkMaster);
};

// NOTE: These methods are defined inline here, so we can share the compilation
//       of visitedlink_master.cc between the browser and the unit/perf tests.

#if defined(UNIT_TEST) || defined(PERF_TEST) || !defined(NDEBUG)
inline void VisitedLinkMaster::DebugValidate() {
  int32 used_count = 0;
  for (int32 i = 0; i < table_length_; i++) {
    if (hash_table_[i])
      used_count++;
  }
  DCHECK(used_count == used_items_);
}
#endif

#endif // CHROME_BROWSER_VISITEDLINK_MASTER_H__

