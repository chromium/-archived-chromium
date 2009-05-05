// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_util.h"

#include "base/hmac.h"
#include "base/logging.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/browser/google_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/base64.h"
#include "net/base/escape.h"
#include "unicode/locid.h"

static const int kSafeBrowsingMacDigestSize = 20;

// Continue to this URL after submitting the phishing report form.
// TODO(paulg): Change to a Chrome specific URL.
static const char kContinueUrlFormat[] =
  "http://www.google.com/tools/firefox/toolbar/FT2/intl/%s/submit_success.html";

static const char kReportParams[] = "?tpl=generic&continue=%s&url=%s";

namespace safe_browsing_util {

const char kMalwareList[] = "goog-malware-shavar";
const char kPhishingList[] = "goog-phish-shavar";

int GetListId(const std::string& name) {
  if (name == kMalwareList)
    return MALWARE;
  else if (name == kPhishingList)
    return PHISH;

  return -1;
}

std::string GetListName(int list_id) {
  switch (list_id) {
    case MALWARE:
      return kMalwareList;
    case PHISH:
      return kPhishingList;
    default:
      return "";
  }
}

void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts) {
  // Per Safe Browsing Protocol 2 spec, first we try the host.  Then we try up
  // to 4 hostnames starting with the last 5 components and successively
  // removing the leading component.  The TLD is skipped.
  hosts->clear();
  int hostnames_checked = 0;

  std::string host = url.host();
  if (host.empty())
    return;

  const char* host_start = host.c_str();
  const char* index = host_start + host.size() - 1;
  bool skipped_tld = false;
  while (index != host_start && hostnames_checked < 4) {
    if (*index == '.') {
      if (!skipped_tld) {
        skipped_tld = true;
      } else {
        const char* host_to_check = index + 1;
        hosts->push_back(host_to_check);
        hostnames_checked++;
      }
    }

    index--;
  }

  // Check the full host too.
  hosts->push_back(host.c_str());
}

// Per the Safe Browsing 2 spec, we try the exact path with/without the query
// parameters, and also the 4 paths formed by starting at the root and adding
// more path components.
void  GeneratePathsToCheck(const GURL& url, std::vector<std::string>* paths) {
  paths->clear();
  std::string path = url.path();
  if (path.empty())
    return;

  if (url.has_query())
    paths->push_back(path + "?" + url.query());

  paths->push_back(path);
  if (path == "/")
    return;

  int path_components_checked = 0;
  const char* path_start = path.c_str();
  const char* index = path_start;
  const char* last_char = path_start + path.size() - 1;
  while (*index && index != last_char && path_components_checked < 4) {
    if (*index == '/') {
      paths->push_back(std::string(path_start, index - path_start + 1));
      path_components_checked++;
    }

    index++;
  }
}

int CompareFullHashes(const GURL& url,
                      const std::vector<SBFullHashResult>& full_hashes) {
  if (full_hashes.empty())
    return -1;

  std::vector<std::string> hosts, paths;
  GenerateHostsToCheck(url, &hosts);
  GeneratePathsToCheck(url, &paths);

  for (size_t h = 0; h < hosts.size(); ++h) {
    for (size_t p = 0; p < paths.size(); ++p) {
      SBFullHash key;
      base::SHA256HashString(hosts[h] + paths[p],
                             key.full_hash,
                             sizeof(SBFullHash));

      for (size_t i = 0; i < full_hashes.size(); ++i) {
        if (key == full_hashes[i].hash)
          return static_cast<int>(i);
      }
    }
  }

  return -1;
}

bool IsPhishingList(const std::string& list_name) {
  return list_name.find("-phish-") != std::string::npos;
}

bool IsMalwareList(const std::string& list_name) {
  return list_name.find("-malware-") != std::string::npos;
}

static void DecodeWebSafe(std::string* decoded) {
  DCHECK(decoded);
  for (size_t i = 0; i < decoded->size(); ++i) {
    switch ((*decoded)[i]) {
      case '_':
        (*decoded)[i] = '/';
        break;
      case '-':
        (*decoded)[i] = '+';
        break;
    }
  }
}

bool VerifyMAC(const std::string& key, const std::string& mac,
               const char* data, int data_length) {
  std::string key_copy = key;
  DecodeWebSafe(&key_copy);
  std::string decoded_key;
  net::Base64Decode(key_copy, &decoded_key);

  std::string mac_copy = mac;
  DecodeWebSafe(&mac_copy);
  std::string decoded_mac;
  net::Base64Decode(mac_copy, &decoded_mac);

  base::HMAC hmac(base::HMAC::SHA1);
  if (!hmac.Init(decoded_key))
    return false;
  const std::string data_str(data, data_length);
  unsigned char digest[kSafeBrowsingMacDigestSize];
  if (!hmac.Sign(data_str, digest, kSafeBrowsingMacDigestSize))
    return false;

  return memcmp(digest, decoded_mac.data(), kSafeBrowsingMacDigestSize) == 0;
}

void FreeChunks(std::deque<SBChunk>* chunks) {
  while (!chunks->empty()) {
    while (!chunks->front().hosts.empty()) {
      chunks->front().hosts.front().entry->Destroy();
      chunks->front().hosts.pop_front();
    }
    chunks->pop_front();
  }
}

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report) {
  Locale locale = Locale::getDefault();
  const char* lang = locale.getLanguage();
  if (!lang)
    lang = "en";  // fallback
  const std::string continue_esc =
      EscapeQueryParamValue(StringPrintf(kContinueUrlFormat, lang));
  const std::string current_esc = EscapeQueryParamValue(url_to_report);
  const std::string format = report_page + kReportParams;
  GURL report_url(StringPrintf(format.c_str(),
                               continue_esc.c_str(),
                               current_esc.c_str()));
  return google_util::AppendGoogleLocaleParam(report_url);
}

}  // namespace safe_browsing_util

const int SBEntry::kMinSize = sizeof(SBEntry::Data);

SBEntry* SBEntry::Create(Type type, int prefix_count) {
  int size = Size(type, prefix_count);
  SBEntry *rv = static_cast<SBEntry*>(malloc(size));
  memset(rv, 0, size);
  rv->set_type(type);
  rv->set_prefix_count(prefix_count);
  return rv;
}

void SBEntry::Destroy() {
  free(this);
}

bool SBEntry::IsValid() const {
  switch (type()) {
    case ADD_PREFIX:
    case ADD_FULL_HASH:
    case SUB_PREFIX:
    case SUB_FULL_HASH:
      return true;
    default:
      return false;
  }
}

int SBEntry::Size() const {
  return Size(type(), prefix_count());
}

int SBEntry::Size(Type type, int prefix_count) {
  return sizeof(Data) + prefix_count * PrefixSize(type);
}

SBEntry* SBEntry::Enlarge(int extra_prefixes) {
  int new_prefix_count = prefix_count() + extra_prefixes;
  SBEntry* rv = SBEntry::Create(type(), new_prefix_count);
  memcpy(rv, this, Size());
  rv->set_prefix_count(new_prefix_count);
  Destroy();
  return rv;
}

void SBEntry::RemovePrefix(int index) {
  DCHECK(index < prefix_count());
  int bytes_to_copy = PrefixSize(type()) * (prefix_count() - index - 1);
  void* to;
  switch (type()) {
    case ADD_PREFIX:
      to = &add_prefixes_[index];
      break;
    case ADD_FULL_HASH:
      to = &add_full_hashes_[index];
      break;
    case SUB_PREFIX:
      to = &sub_prefixes_[index];
      break;
    case SUB_FULL_HASH:
      to = &sub_full_hashes_[index];
      break;
    default:
      NOTREACHED();
      return;
  }

  char* from = reinterpret_cast<char*>(to) + PrefixSize(type());
  memmove(to, from, bytes_to_copy);
  set_prefix_count(prefix_count() - 1);
}

bool SBEntry::PrefixesMatch(
    int index, const SBEntry* that, int that_index)  const {
  // If they're of different hash sizes, or if they're both adds or subs, then
  // they can't match.
  if (HashLen() != that->HashLen() || IsAdd() == that->IsAdd())
    return false;

  if (ChunkIdAtPrefix(index) != that->ChunkIdAtPrefix(that_index))
    return false;

  if (HashLen() == sizeof(SBPrefix))
    return PrefixAt(index) == that->PrefixAt(that_index);

  return FullHashAt(index) == that->FullHashAt(that_index);
}

bool SBEntry::AddPrefixMatches(int index, const SBFullHash& full_hash) const {
  DCHECK(IsAdd());

  if (HashLen() == sizeof(SBFullHash))
    return full_hash == add_full_hashes_[index];

  SBPrefix prefix;
  memcpy(&prefix, &full_hash, sizeof(SBPrefix));
  return prefix == add_prefixes_[index];
}

bool SBEntry::IsAdd() const {
  return type() == ADD_PREFIX || type() == ADD_FULL_HASH;
}

bool SBEntry::IsSub() const {
  return type() == SUB_PREFIX || type() == SUB_FULL_HASH;
}

int SBEntry::HashLen() const {
  if (type() == ADD_PREFIX || type() == SUB_PREFIX)
    return sizeof(SBPrefix);

  return sizeof(SBFullHash);
}

int SBEntry::PrefixSize(Type type) {
  switch (type) {
    case ADD_PREFIX:
      return sizeof(SBPrefix);
    case ADD_FULL_HASH:
      return sizeof(SBFullHash);
    case SUB_PREFIX:
      return sizeof(SBSubPrefix);
    case SUB_FULL_HASH:
      return sizeof(SBSubFullHash);
    default:
      NOTREACHED();
      return 0;
  }
}

int SBEntry::ChunkIdAtPrefix(int index) const {
  if (type() == SUB_PREFIX)
    return sub_prefixes_[index].add_chunk;

  if (type() == SUB_FULL_HASH)
    return sub_full_hashes_[index].add_chunk;

  return chunk_id();
}

void SBEntry::SetChunkIdAtPrefix(int index, int chunk_id) {
  DCHECK(IsSub());

  if (type() == SUB_PREFIX) {
    sub_prefixes_[index].add_chunk = chunk_id;
  } else {
    sub_full_hashes_[index].add_chunk = chunk_id;
  }
}

const SBPrefix& SBEntry::PrefixAt(int index) const {
  DCHECK(HashLen() == sizeof(SBPrefix));

  if (IsAdd())
    return add_prefixes_[index];

  return sub_prefixes_[index].prefix;
}

const SBFullHash& SBEntry::FullHashAt(int index) const {
  DCHECK(HashLen() == sizeof(SBFullHash));

  if (IsAdd())
    return add_full_hashes_[index];

  return sub_full_hashes_[index].prefix;
}

void SBEntry::SetPrefixAt(int index, const SBPrefix& prefix) {
  DCHECK(HashLen() == sizeof(SBPrefix));

  if (IsAdd()) {
    add_prefixes_[index] = prefix;
  } else {
    sub_prefixes_[index].prefix = prefix;
  }
}

void SBEntry::SetFullHashAt(int index, const SBFullHash& full_hash) {
  DCHECK(HashLen() == sizeof(SBFullHash));

  if (IsAdd()) {
    add_full_hashes_[index] = full_hash;
  } else {
    sub_full_hashes_[index].prefix = full_hash;
  }
}



SBHostInfo::SBHostInfo() : size_(0) {
}

bool SBHostInfo::Initialize(const void* data, int size) {
  size_ = size;
  if (!size_)
    return true;

  data_.reset(new char[size_]);
  memcpy(data_.get(), data, size_);
  if (!IsValid()) {
    size_ = 0;
    data_.reset();
    return false;
  }

  return true;
}

bool SBHostInfo::IsValid() {
  const SBEntry* entry = NULL;
  while (GetNextEntry(&entry)) {
    if (!entry->IsValid())
      return false;
  }
  return true;
}

void SBHostInfo::Add(const SBEntry* entry) {
  int new_size = size_ + entry->Size();
  char* new_data = new char[new_size];
  memcpy(new_data, data_.get(), size_);
  memcpy(new_data + size_, entry, entry->Size());
  data_.reset(new_data);
  size_ = new_size;
  DCHECK(IsValid());
}

void SBHostInfo::AddPrefixes(SBEntry* entry) {
  DCHECK(entry->IsAdd());
  bool insert_entry = true;
  const SBEntry* sub_entry = NULL;
  // Remove any prefixes for which a sub already came.
  while (GetNextEntry(&sub_entry)) {
    if (sub_entry->IsAdd() || entry->list_id() != sub_entry->list_id())
      continue;

    if (sub_entry->prefix_count() == 0) {
      if (entry->chunk_id() != sub_entry->chunk_id())
        continue;

      // We don't want to add any of these prefixes so just return.  Also no
      // more need to store the sub chunk data around for this chunk_id so
      // remove it.
      RemoveSubEntry(entry->list_id(), entry->chunk_id());
      return;
    }

    // Remove any matching prefixes.
    for (int i = 0; i < sub_entry->prefix_count(); ++i) {
      for (int j = 0; j < entry->prefix_count(); ++j) {
        if (entry->PrefixesMatch(j, sub_entry, i)) {
          entry->RemovePrefix(j--);
          if (!entry->prefix_count()) {
            // The add entry used to have prefixes, but they were all removed
            // because of matching sub entries.  We don't want to add this
            // empty add entry, because it would block that entire host.
            insert_entry = false;
          }
        }
      }
    }

    RemoveSubEntry(entry->list_id(), entry->chunk_id());
    break;
  }

  if (insert_entry)
    Add(entry);
  DCHECK(IsValid());
}

void SBHostInfo::RemoveSubEntry(int list_id, int chunk_id) {
  scoped_array<char> new_data(new char[size_]);  // preallocate new data
  char* write_ptr = new_data.get();
  int new_size = 0;
  const SBEntry* entry = NULL;
  while (GetNextEntry(&entry)) {
    if (entry->list_id() == list_id &&
        entry->chunk_id() == chunk_id &&
        entry->IsSub() &&
        entry->prefix_count() == 0) {
      continue;
    }

    SBEntry* new_sub_entry = const_cast<SBEntry*>(entry);
    scoped_array<char> data;
    if (entry->IsSub() && entry->list_id() == list_id &&
        entry->prefix_count()) {
      // Make a copy of the entry so that we can modify it.
      data.reset(new char[entry->Size()]);
      new_sub_entry = reinterpret_cast<SBEntry*>(data.get());
      memcpy(new_sub_entry, entry, entry->Size());
      // Remove any matching prefixes.
      for (int i = 0; i < new_sub_entry->prefix_count(); ++i) {
        if (new_sub_entry->ChunkIdAtPrefix(i) == chunk_id)
          new_sub_entry->RemovePrefix(i--);
      }

      if (new_sub_entry->prefix_count() == 0)
        continue;  // We removed the last prefix in the entry, so remove it.
    }

    memcpy(write_ptr, new_sub_entry, new_sub_entry->Size());
    new_size += new_sub_entry->Size();
    write_ptr += new_sub_entry->Size();
  }

  size_ = new_size;
  data_.reset(new_data.release());
  DCHECK(IsValid());
}

void SBHostInfo::RemovePrefixes(SBEntry* sub_entry, bool persist) {
  DCHECK(sub_entry->IsSub());
  scoped_array<char> new_data(new char[size_]);
  char* write_ptr = new_data.get();
  int new_size = 0;
  const SBEntry* add_entry = NULL;
  // Remove any of the prefixes that are in the database.
  while (GetNextEntry(&add_entry)) {
    SBEntry* new_add_entry = const_cast<SBEntry*>(add_entry);
    scoped_array<char> data;
    if (add_entry->IsAdd() && add_entry->list_id() == sub_entry->list_id()) {
      if (sub_entry->prefix_count() == 0 &&
          add_entry->chunk_id() == sub_entry->chunk_id()) {
        // When prefixes are empty, that means we want to remove the entry for
        // that host key completely.  No need to add this sub chunk to the db.
        persist = false;
        continue;
      } else if (sub_entry->prefix_count() && add_entry->prefix_count()) {
        // Remove any of the sub prefixes from these add prefixes.
        data.reset(new char[add_entry->Size()]);
        new_add_entry = reinterpret_cast<SBEntry*>(data.get());
        memcpy(new_add_entry, add_entry, add_entry->Size());

        for (int i = 0; i < new_add_entry->prefix_count(); ++i) {
          for (int j = 0; j < sub_entry->prefix_count(); ++j) {
            if (!sub_entry->PrefixesMatch(j, new_add_entry, i))
              continue;

            new_add_entry->RemovePrefix(i--);
            sub_entry->RemovePrefix(j--);
            if (sub_entry->prefix_count() == 0)
              persist = false;  // Sub entry is all used up.

            break;
          }
        }
      }
    }

    // If we didn't modify the entry, then add it.  Else if we modified it,
    // then only add it if there are prefixes left.  Otherwise, it it had n
    // prefixes and now it has 0, if we were to add it that would mean all
    // prefixes from that host are in the database.
    if (new_add_entry == add_entry || new_add_entry->prefix_count()) {
      memcpy(write_ptr, new_add_entry, new_add_entry->Size());
      new_size += new_add_entry->Size();
      write_ptr += new_add_entry->Size();
    }
  }

  if (persist && new_size == size_) {
    // We didn't find any matches because the sub came before the add, so save
    // it for later.
    Add(sub_entry);
    return;
  }

  size_ = new_size;
  data_.reset(new_data.release());
  DCHECK(IsValid());
}

bool SBHostInfo::Contains(const std::vector<SBFullHash>& prefixes,
                          int* list_id,
                          std::vector<SBPrefix>* prefix_hits) {
  prefix_hits->clear();
  *list_id = -1;
  bool hits = false;
  const SBEntry* add_entry = NULL;
  while (GetNextEntry(&add_entry)) {
    if (add_entry->IsSub())
      continue;

    if (add_entry->prefix_count() == 0) {
      // This means all paths for this url are blacklisted.
      return true;
    }

    for (int i = 0; i < add_entry->prefix_count(); ++i) {
      for (size_t j = 0; j < prefixes.size(); ++j) {
        if (!add_entry->AddPrefixMatches(i, prefixes[j]))
          continue;

        hits = true;
        if (add_entry->HashLen() == sizeof(SBFullHash)) {
          *list_id = add_entry->list_id();
        } else {
          prefix_hits->push_back(add_entry->PrefixAt(i));
        }
      }
    }
  }

  return hits;
}

bool SBHostInfo::GetNextEntry(const SBEntry** entry) {
  const char* current = reinterpret_cast<const char*>(*entry);

  // It is an error to call this function with a |*entry| outside of |data_|.
  DCHECK(!current || current >= data_.get());
  DCHECK(!current || current + (*entry)->Size() <= data_.get() + size_);

  // Compute the address of the next entry.
  const char* next = current ? current + (*entry)->Size() : data_.get();
  const SBEntry* next_entry = reinterpret_cast<const SBEntry*>(next);

  // Validate that the next entry is wholly contained inside of |data_|.
  const char* end = data_.get() + size_;
  if (next + SBEntry::kMinSize <= end && next + next_entry->Size() <= end) {
    *entry = next_entry;
    return true;
  }

  return false;
}
