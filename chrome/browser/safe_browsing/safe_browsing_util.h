// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_

#include <string.h>

#include <deque>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/safe_browsing/chunk_range.h"

class GURL;

//#define SB_LOGGING_ENABLED
#ifdef SB_LOGGING_ENABLED
#define SB_DLOG(severity) DLOG_IF(INFO, 1)
#else
#define SB_DLOG(severity) DLOG_IF(INFO, 0)
#endif

// forward declaration
class SBEntry;

// Widely used typedefs -------------------------------------------------------

// Container for holding a chunk URL and the MAC of the contents of the URL.
typedef struct {
  std::string url;
  std::string mac;
  std::string list_name;
} ChunkUrl;

// A truncated hash's type.
typedef int SBPrefix;

// A full hash.
typedef struct {
  char full_hash[32];
} SBFullHash;

inline bool operator==(const SBFullHash& rhash, const SBFullHash& lhash) {
  return memcmp(rhash.full_hash, lhash.full_hash, sizeof(SBFullHash)) == 0;
}

// Container for information about a specific host in an add/sub chunk.
struct SBChunkHost {
  SBPrefix host;
  SBEntry* entry;
};

// Container for an add/sub chunk.
struct SBChunk {
  int chunk_number;
  int list_id;
  bool is_add;
  std::deque<SBChunkHost> hosts;
};

// Used when we get a gethash response.
struct SBFullHashResult {
  SBFullHash hash;
  std::string list_name;
  int add_chunk_id;
};

// Contains information about a list in the database.
struct SBListChunkRanges {
  std::string name;  // The list name.
  std::string adds;  // The ranges for add chunks.
  std::string subs;  // The ranges for sub chunks.

  SBListChunkRanges(const std::string& n) : name(n) { }
};

// Container for deleting chunks from the database.
struct SBChunkDelete {
  std::string list_name;
  bool is_sub_del;
  std::vector<ChunkRange> chunk_del;
};


// Holds information about the prefixes for a hostkey.  prefixes can either be
// 4 bytes (truncated hash) or 32 bytes (full hash).
// For adds:
//   [list id ][chunk id][prefix count (0..n)][prefix1][prefix2]
// For subs:
//   [list id ][chunk id (only used if prefix count is 0][prefix count (0..n)]
//       [add chunk][prefix][add chunk][prefix]
class SBEntry {
 public:
  enum Type {
    ADD_PREFIX,     // 4 byte add entry.
    SUB_PREFIX,     // 4 byte sub entry.
    ADD_FULL_HASH,  // 32 byte add entry.
    SUB_FULL_HASH,  // 32 byte sub entry.
  };

  // The minimum size of an SBEntry.
  static const int kMinSize;

  // Creates a SBEntry with the necessary size for the given number of prefixes.
  // Caller ownes the object and needs to free it by calling Destroy.
  static SBEntry* Create(Type type, int prefix_count);

  // Frees the entry's memory.
  void Destroy();

  // Returns whether this entry is internally consistent.
  bool IsValid() const;

  // Returns how many bytes this entry is.
  int Size() const;

  // Helper to return how much memory a given Entry would require.
  static int Size(Type type, int prefix_count);

  void set_list_id(int list_id) { data_.list_id = list_id; }
  int list_id() const { return data_.list_id; }
  void set_chunk_id(int chunk_id) { data_.chunk_id = chunk_id; }
  int chunk_id() const { return data_.chunk_id; }
  int prefix_count() const { return data_.prefix_count; }
  Type type() const { return data_.type; }

  // Returns a new entry that is larger by the given number of prefixes, with
  // all the existing data already copied over.  The old entry is destroyed.
  SBEntry* Enlarge(int extra_prefixes);

  // Removes the prefix at the given index.
  void RemovePrefix(int index);

  // Returns true if the prefix/hash at the given index is equal to a
  // prefix/hash at another entry's index.  Works with all combinations of
  // add/subs as long as they're the same size.  Also checks chunk_ids.
  bool PrefixesMatch(int index, const SBEntry* that, int that_index) const;

  // Returns true if the add prefix/hash at the given index is equal to the
  // given full hash.
  bool AddPrefixMatches(int index, const SBFullHash& full_hash) const;

  // Returns true if this is an add entry.
  bool IsAdd() const;

  // Returns true if this is a sub entry.
  bool IsSub() const;

  // Helper to return the size of the prefixes.
  int HashLen() const;

  // Helper to return the size of each prefix entry (i.e. for subs this
  // includes an add chunk id).
  static int PrefixSize(Type type);

  // For add entries, returns the add chunk id.  For sub entries, returns the
  // add_chunk id for the prefix at the given index.
  int ChunkIdAtPrefix(int index) const;

  // Used for sub chunks to set the chunk id at a given index.
  void SetChunkIdAtPrefix(int index, int chunk_id);

  // Return the prefix/full hash at the given index.  Caller is expected to
  // call the right function based on the hash length.
  const SBPrefix& PrefixAt(int index) const;
  const SBFullHash& FullHashAt(int index) const;

  // Return the prefix/full hash at the given index.  Caller is expected to
  // call the right function based on the hash length.
  void SetPrefixAt(int index, const SBPrefix& prefix);
  void SetFullHashAt(int index, const SBFullHash& full_hash);

 private:
  SBEntry();
  ~SBEntry();

  void set_prefix_count(int count) { data_.prefix_count = count; }
  void set_type(Type type) { data_.type = type; }

  // Container for a sub prefix.
  struct SBSubPrefix {
    int add_chunk;
    SBPrefix prefix;
  };

  // Container for a sub full hash.
  struct SBSubFullHash {
    int add_chunk;
    SBFullHash prefix;
  };

  // Keep the fixed data together in one struct so that we can get its size
  // easily.  If any of this is modified, the database will have to be cleared.
  struct Data {
    int list_id;
    // For adds, this is the add chunk number.
    // For subs: if prefix_count is 0 then this is the add chunk that this sub
    //     refers to.  Otherwise it's ignored, and the add_chunk in sub_prefixes
    //     or sub_full_hashes is used for each corresponding prefix.
    int chunk_id;
    Type type;
    int prefix_count;
  };

  // The prefixes union must follow the fixed data so that they're contiguous
  // in memory.
  Data data_;
  union {
    SBPrefix add_prefixes_[1];
    SBSubPrefix sub_prefixes_[1];
    SBFullHash add_full_hashes_[1];
    SBSubFullHash sub_full_hashes_[1];
  };
};


// Holds the hostkey specific information in the database.  This is basically a
// collection of SBEntry objects.
class SBHostInfo {
 public:
  SBHostInfo();
  // By default, an empty SBHostInfo is created.  Call this to deserialize from
  // the database.  Returns false if |data| is not internally consistent.
  bool Initialize(const void* data, int size);

  // Adds the given prefixes to the unsafe list.  Note that the prefixes array
  // might be modified internally.
  void AddPrefixes(SBEntry* entry);

  // Remove the given prefixes.  If prefixes is empty, then all entries from
  // sub.add_chunk_number are removed.  Otherwise sub. add_chunk_id is ignored
  // and the chunk_id from each element in sub.prefixes is checked.  If persist
  // is true and no matches are found, then the sub information will be stored
  // and checked in case a future add comes in with that chunk_id.
  void RemovePrefixes(SBEntry* entry, bool persist);

  // Returns true if the host entry contains any of the prefixes.  If a full
  // hash matched, then list_id contains the list id.  Otherwise list_id is -1
  // and prefix_hits contains the matching prefixes if any are matched, or is
  // empty if the entire host is blacklisted.
  bool Contains(const std::vector<SBFullHash>& prefixes,
                int* list_id,
                std::vector<SBPrefix>* prefix_hits);

  // Used for serialization.
  const void* data() const { return data_.get(); }
  const int size() const { return size_; }

 private:
  // Checks data_ for internal consistency.
  bool IsValid();

  // Allows enumeration of Entry structs.  To start off, pass NULL for *entry,
  // and then afterwards return the previous pointer.
  bool GetNextEntry(const SBEntry** entry);

  void Add(const SBEntry* entry);

  void RemoveSubEntry(int list_id, int chunk_id);

  // Collection of SBEntry objects.
  scoped_array<char> data_;
  int size_;
};


// Helper functions -----------------------------------------------------------

namespace safe_browsing_util {

// SafeBrowsing list names.
extern const char kMalwareList[];
extern const char kPhishingList[];

// Converts between the SafeBrowsing list names and their enumerated value.
// If the list names change, both of these methods must be updated.
enum ListType {
  MALWARE = 0,
  PHISH = 1,
};
int GetListId(const std::string& name);
std::string GetListName(int list_id);

void FreeChunks(std::deque<SBChunk>* chunks);

// Given a URL, returns all the hosts we need to check.  They are returned
// in order of size (i.e. b.c is first, then a.b.c).
void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts);

// Given a URL, returns all the paths we need to check.
void GeneratePathsToCheck(const GURL& url, std::vector<std::string>* paths);

// Given a URL, compare all the possible host + path full hashes to the set of
// provided full hashes.  Returns the index of the match if one is found, or -1
// otherwise.
int CompareFullHashes(const GURL& url,
                      const std::vector<SBFullHashResult>& full_hashes);

bool IsPhishingList(const std::string& list_name);
bool IsMalwareList(const std::string& list_name);

// Returns 'true' if 'mac' can be verified using 'key' and 'data'.
bool VerifyMAC(const std::string& key,
               const std::string& mac,
               const char* data,
               int data_length);

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report);

}  // namespace safe_browsing_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
