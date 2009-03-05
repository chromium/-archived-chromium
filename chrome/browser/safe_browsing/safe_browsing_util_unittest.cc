// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/sha2.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  bool VectorContains(const std::vector<std::string>& data,
                      const std::string& str) {
    for (size_t i = 0; i < data.size(); ++i) {
      if (data[i] == str)
        return true;
    }

    return false;
  }

SBFullHash CreateFullHash(SBPrefix prefix) {
  SBFullHash result;
  memset(&result, 0, sizeof(result));
  memcpy(&result, &prefix, sizeof(result));
  return result;
}
}

// Tests that we generate the required host/path combinations for testing
// according to the Safe Browsing spec.
// See section 6.2 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
TEST(SafeBrowsingUtilTest, UrlParsing) {
  std::vector<std::string> hosts, paths;

  GURL url("http://a.b.c/1/2.html?param=1");
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(paths.size(), static_cast<size_t>(4));
  EXPECT_EQ(hosts[0], "b.c");
  EXPECT_EQ(hosts[1], "a.b.c");

  EXPECT_TRUE(VectorContains(paths, "/1/2.html?param=1"));
  EXPECT_TRUE(VectorContains(paths, "/1/2.html"));
  EXPECT_TRUE(VectorContains(paths, "/1/"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b.c.d.e.f.g/1.html");
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(5));
  EXPECT_EQ(paths.size(), static_cast<size_t>(2));
  EXPECT_EQ(hosts[0], "f.g");
  EXPECT_EQ(hosts[1], "e.f.g");
  EXPECT_EQ(hosts[2], "d.e.f.g");
  EXPECT_EQ(hosts[3], "c.d.e.f.g");
  EXPECT_EQ(hosts[4], "a.b.c.d.e.f.g");
  EXPECT_TRUE(VectorContains(paths, "/1.html"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b/saw-cgi/eBayISAPI.dll/");
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(paths.size(), static_cast<size_t>(3));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/eBayISAPI.dll/"));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/"));
  EXPECT_TRUE(VectorContains(paths, "/"));
}


TEST(SafeBrowsingUtilTest, FullHashCompare) {
  GURL url("http://www.evil.com/phish.html");
  SBFullHashResult full_hash;
  base::SHA256HashString(url.host() + url.path(),
                         &full_hash.hash,
                         sizeof(SBFullHash));
  std::vector<SBFullHashResult> full_hashes;
  full_hashes.push_back(full_hash);

  EXPECT_EQ(safe_browsing_util::CompareFullHashes(url, full_hashes), 0);

  url = GURL("http://www.evil.com/okay_path.html");
  EXPECT_EQ(safe_browsing_util::CompareFullHashes(url, full_hashes), -1);
}

// Checks the reading/writing code of the database information for a hostkey.
TEST(SafeBrowsing, HostInfo) {
  // Test a simple case of adding a prefix from scratch.
  SBEntry* entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  entry->SetPrefixAt(0, 0x01000000);
  entry->set_list_id(1);
  entry->set_chunk_id(1);

  SBHostInfo info;
  info.AddPrefixes(entry);
  entry->Destroy();

  int list_id;
  std::vector<SBFullHash> full_hashes;
  full_hashes.push_back(CreateFullHash(0x01000000));
  std::vector<SBPrefix> prefix_hits;
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Test appending prefixes to an existing entry.
  entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  entry->SetPrefixAt(0, 0x02000000);
  entry->SetPrefixAt(1, 0x02000001);
  entry->set_list_id(1);
  entry->set_chunk_id(2);
  info.AddPrefixes(entry);
  entry->Destroy();

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x01000000));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000000));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000001));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));


  // Test removing the entire first entry.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 0);
  entry->set_list_id(1);
  entry->set_chunk_id(1);
  info.RemovePrefixes(entry, false);
  entry->Destroy();

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x01000000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000000));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000001));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Test removing one prefix from the second entry.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  entry->SetPrefixAt(0,0x02000000);
  entry->SetChunkIdAtPrefix(0, 2);
  entry->set_list_id(1);
  info.RemovePrefixes(entry, false);
  entry->Destroy();

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x02000001));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Test adding a sub that specifies a prefix before the add.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  entry->SetPrefixAt(0, 0x1000);
  entry->SetChunkIdAtPrefix(0, 100);
  entry->set_list_id(1);
  info.RemovePrefixes(entry, true);
  entry->Destroy();

  // Make sure we don't get a match from a sub.
  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x1000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Now add the prefixes.
  entry = SBEntry::Create(SBEntry::ADD_PREFIX, 3);
  entry->SetPrefixAt(0, 0x10000);
  entry->SetPrefixAt(1, 0x1000);
  entry->SetPrefixAt(2, 0x100000);
  entry->set_list_id(1);
  entry->set_chunk_id(100);
  info.AddPrefixes(entry);
  entry->Destroy();

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x10000));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x1000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x100000));
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Now try adding a sub that deletes all prefixes from the chunk.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 0);
  entry->set_list_id(1);
  entry->set_chunk_id(100);
  info.RemovePrefixes(entry, true);
  entry->Destroy();

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x10000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x100000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Add a sub for all prefixes before the add comes.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 0);
  entry->set_list_id(1);
  entry->set_chunk_id(200);
  info.RemovePrefixes(entry, true);
  entry->Destroy();

  // Now add the prefixes.
  entry = SBEntry::Create(SBEntry::ADD_PREFIX, 3);
  entry->SetPrefixAt(0, 0x2000);
  entry->SetPrefixAt(1, 0x20000);
  entry->SetPrefixAt(2, 0x200000);
  entry->set_list_id(1);
  entry->set_chunk_id(200);
  info.AddPrefixes(entry);
  entry->Destroy();

  // None of the prefixes should be found.
  full_hashes.clear();
  full_hashes.push_back(CreateFullHash(0x2000));
  full_hashes.push_back(CreateFullHash(0x20000));
  full_hashes.push_back(CreateFullHash(0x200000));
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));
}

// Checks that if we have a hostname blacklisted and we get a sub prefix, the
// hostname remains blacklisted.
TEST(SafeBrowsing, HostInfo2) {
  // Blacklist the entire hostname.
  SBEntry* entry = SBEntry::Create(SBEntry::ADD_PREFIX, 0);
  entry->set_list_id(1);
  entry->set_chunk_id(1);

  SBHostInfo info;
  info.AddPrefixes(entry);
  entry->Destroy();

  int list_id;
  std::vector<SBFullHash> full_hashes;
  full_hashes.push_back(CreateFullHash(0x01000000));
  std::vector<SBPrefix> prefix_hits;
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Now add a sub prefix.
  entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  entry->SetPrefixAt(0, 0x02000000);
  entry->SetChunkIdAtPrefix(0, 2);
  entry->set_list_id(1);
  info.RemovePrefixes(entry, true);
  entry->Destroy();

  // Any prefix except the one removed should still be blocked.
  EXPECT_TRUE(info.Contains(full_hashes, &list_id, &prefix_hits));
}

// Checks that if we get a sub chunk with one prefix, then get the add chunk
// for that same prefix afterwards, the entry becomes empty.
TEST(SafeBrowsing, HostInfo3) {
  SBHostInfo info;

  // Add a sub prefix.
  SBEntry* entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  entry->SetPrefixAt(0, 0x01000000);
  entry->SetChunkIdAtPrefix(0, 1);
  entry->set_list_id(1);
  info.RemovePrefixes(entry, true);
  entry->Destroy();

  int list_id;
  std::vector<SBFullHash> full_hashes;
  full_hashes.push_back(CreateFullHash(0x01000000));
  std::vector<SBPrefix> prefix_hits;
  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));

  // Now add the prefix.
  entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  entry->SetPrefixAt(0, 0x01000000);
  entry->set_list_id(1);
  entry->set_chunk_id(1);
  info.AddPrefixes(entry);
  entry->Destroy();

  EXPECT_FALSE(info.Contains(full_hashes, &list_id, &prefix_hits));
}
