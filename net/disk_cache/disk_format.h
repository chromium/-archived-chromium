// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The cache is stored on disk as a collection of block-files, plus an index
// file plus a collection of external files.
//
// Any data blob bigger than kMaxBlockSize (net/addr.h) will be stored on a
// separate file named f_xxx where x is a hexadecimal number. Shorter data will
// be stored as a series of blocks on a block-file. In any case, CacheAddr
// represents the address of the data inside the cache.
//
// The index file is just a simple hash table that maps a particular entry to
// a CacheAddr value. Linking for a given hash bucket is handled internally
// by the cache entry.
//
// The last element of the cache is the block-file. A block file is a file
// designed to store blocks of data of a given size. It is able to store data
// that spans from one to four consecutive "blocks", and it grows as needed to
// store up to approximately 65000 blocks. It has a fixed size header used for
// book keeping such as tracking free of blocks on the file. For example, a
// block-file for 1KB blocks will grow from 8KB when totally empty to about 64MB
// when completely full. At that point, data blocks of 1KB will be stored on a
// second block file that will store the next set of 65000 blocks. The first
// file contains the number of the second file, and the second file contains the
// number of a third file, created when the second file reaches its limit. It is
// important to remember that no matter how long the chain of files is, any
// given block can be located directly by its address, which contains the file
// number and starting block inside the file.
//
// A new cache is initialized with four block files (named data_0 through
// data_3), each one dedicated to store blocks of a given size. The number at
// the end of the file name is the block file number (in decimal).
//
// There are two "special" types of blocks: an entry and a rankings node. An
// entry keeps track of all the information related to the same cache entry,
// such as the key, hash value, data pointers etc. A rankings node keeps track
// of the information that is updated frequently for a given entry, such as its
// location on the LRU lists, last access time etc.
//
// The files that store internal information for the cache (blocks and index)
// are at least partially memory mapped. They have a location that is signaled
// every time the internal structures are modified, so it is possible to detect
// (most of the time) when the process dies in the middle of an update.
//
// In order to prevent dirty data to be used as valid (after a crash), every
// cache entry has a dirty identifier. Each running instance of the cache keeps
// a separate identifier (maintained on the "this_id" header field) that is used
// to mark every entry that is created or modified. When the entry is closed,
// and all the data can be trusted, the dirty flag is cleared from the entry.
// When the cache encounters an entry whose identifier is different than the one
// being currently used, it means that the entry was not properly closed on a
// previous run, so it is discarded.

#ifndef NET_DISK_CACHE_DISK_FORMAT_H_
#define NET_DISK_CACHE_DISK_FORMAT_H_

#include "base/basictypes.h"

namespace disk_cache {

typedef uint32 CacheAddr;

const int kIndexTablesize = 0x10000;
const uint32 kIndexMagic = 0xC103CAC3;
const uint32 kCurrentVersion = 0x20000;  // Version 2.0.

struct LruData {
  int32     pad1[2];
  int32     filled;          // Flag to tell when we filled the cache.
  int32     sizes[5];
  CacheAddr heads[5];
  CacheAddr tails[5];
  CacheAddr transaction;     // In-flight operation target.
  int32     operation;       // Actual in-flight operation.
  int32     operation_list;  // In-flight operation list.
  int32     pad2[7];
};

// Header for the master index file.
struct IndexHeader {
  uint32      magic;
  uint32      version;
  int32       num_entries;   // Number of entries currently stored.
  int32       num_bytes;     // Total size of the stored data.
  int32       last_file;     // Last external file created.
  int32       this_id;       // Id for all entries being changed (dirty flag).
  CacheAddr   stats;         // Storage for usage data.
  int32       table_len;     // Actual size of the table (0 == kIndexTablesize).
  int32       crash;         // Signals a previous crash.
  int32       experiment;    // Id of an ongoing test.
  uint64      create_time;   // Creation time for this set of files.
  int32       pad[52];
  LruData     lru;           // Eviction control data.
  IndexHeader() {
    memset(this, 0, sizeof(*this));
    magic = kIndexMagic;
    version = kCurrentVersion;
  };
};

// The structure of the whole index file.
struct Index {
  IndexHeader header;
  CacheAddr   table[kIndexTablesize];  // Default size. Actual size controlled
                                       // by header.table_len.
};

// Main structure for an entry on the backing storage. If the key is longer than
// what can be stored on this structure, it will be extended on consecutive
// blocks (adding 256 bytes each time), up to 4 blocks (1024 - 32 - 1 chars).
// After that point, the whole key will be stored as a data block or external
// file.
struct EntryStore {
  uint32      hash;               // Full hash of the key.
  CacheAddr   next;               // Next entry with the same hash or bucket.
  CacheAddr   rankings_node;      // Rankings node for this entry.
  int32       reuse_count;        // How often is this entry used.
  int32       refetch_count;      // How often is this fetched from the net.
  int32       state;              // Current state.
  uint64      creation_time;
  int32       key_len;
  CacheAddr   long_key;           // Optional address of a long key.
  int32       data_size[4];       // We can store up to 4 data streams for each
  CacheAddr   data_addr[4];       // entry.
  int32       pad[6];
  char        key[256 - 24 * 4];  // null terminated
};

COMPILE_ASSERT(sizeof(EntryStore) == 256, bad_EntyStore);
const int kMaxInternalKeyLength = 4 * sizeof(EntryStore) -
                                  offsetof(EntryStore, key) - 1;

// Possible states for a given entry.
enum EntryState {
  ENTRY_NORMAL = 0,
  ENTRY_EVICTED,    // The entry was recently evicted from the cache.
  ENTRY_DOOMED      // The entry was doomed.
};

#pragma pack(push, old, 4)
// Rankings information for a given entry.
struct RankingsNode {
  uint64      last_used;        // LRU info.
  uint64      last_modified;    // LRU info.
  CacheAddr   next;             // LRU list.
  CacheAddr   prev;             // LRU list.
  CacheAddr   contents;         // Address of the EntryStore.
  int32       dirty;            // The entry is being modifyied.
  void*       pointer;          // Pointer to the in-memory entry.
};
#pragma pack(pop, old)

COMPILE_ASSERT(sizeof(RankingsNode) == 36, bad_RankingsNode);

const uint32 kBlockMagic = 0xC104CAC3;
const int kBlockHeaderSize = 8192;  // Two pages: almost 64k entries
const int kMaxBlocks = (kBlockHeaderSize - 80) * 8;

// Bitmap to track used blocks on a block-file.
typedef uint32 AllocBitmap[kMaxBlocks / 32];

// A block-file is the file used to store information in blocks (could be
// EntryStore blocks, RankingsNode blocks or user-data blocks).
// We store entries that can expand for up to 4 consecutive blocks, and keep
// counters of the number of blocks available for each type of entry. For
// instance, an entry of 3 blocks is an entry of type 3. We also keep track of
// where did we find the last entry of that type (to avoid searching the bitmap
// from the beginning every time).
// This Structure is the header of a block-file:
struct BlockFileHeader {
  uint32          magic;
  uint32          version;
  int16           this_file;    // Index of this file.
  int16           next_file;    // Next file when this one is full.
  int32           entry_size;   // Size of the blocks of this file.
  int32           num_entries;  // Number of stored entries.
  int32           max_entries;  // Current maximum number of entries.
  int32           empty[4];     // Counters of empty entries for each type.
  int32           hints[4];     // Last used position for each entry type.
  volatile int32  updating;     // Keep track of updates to the header.
  int32           user[5];
  AllocBitmap     allocation_map;
  BlockFileHeader() {
    memset(this, 0, sizeof(BlockFileHeader));
    magic = kBlockMagic;
    version = kCurrentVersion;
  };
};

COMPILE_ASSERT(sizeof(BlockFileHeader) == kBlockHeaderSize, bad_header);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_DISK_FORMAT_H_
