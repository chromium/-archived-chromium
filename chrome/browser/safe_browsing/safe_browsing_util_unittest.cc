// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

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
  EXPECT_EQ(hosts.size(), 2);
  EXPECT_EQ(paths.size(), 4);
  EXPECT_EQ(hosts[0], "b.c");
  EXPECT_EQ(hosts[1], "a.b.c");

  EXPECT_TRUE(VectorContains(paths, "/1/2.html?param=1"));
  EXPECT_TRUE(VectorContains(paths, "/1/2.html"));
  EXPECT_TRUE(VectorContains(paths, "/1/"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b.c.d.e.f.g/1.html");
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), 5);
  EXPECT_EQ(paths.size(), 2);
  EXPECT_EQ(hosts[0], "f.g");
  EXPECT_EQ(hosts[1], "e.f.g");
  EXPECT_EQ(hosts[2], "d.e.f.g");
  EXPECT_EQ(hosts[3], "c.d.e.f.g");
  EXPECT_EQ(hosts[4], "a.b.c.d.e.f.g");
  EXPECT_TRUE(VectorContains(paths, "/1.html"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b/saw-cgi/eBayISAPI.dll/");
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(paths.size(), 3);
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
