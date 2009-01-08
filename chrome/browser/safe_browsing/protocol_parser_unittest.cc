// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Program to test the SafeBrowsing protocol parsing v2.1.

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "testing/gtest/include/gtest/gtest.h"


// Test parsing one add chunk.
TEST(SafeBrowsingProtocolParsingTest, TestAddChunk) {
  std::string add_chunk("a:1:4:35\naaaax1111\0032222333344447777\00288889999");
  add_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(add_chunk.data(),
                                  static_cast<int>(add_chunk.length()),
                                  "", "",  &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(3));

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing one add chunk with full hashes.
TEST(SafeBrowsingProtocolParsingTest, TestAddFullChunk) {
  std::string add_chunk("a:1:32:69\naaaa");
  add_chunk.push_back(2);

  SBFullHash full_hash1, full_hash2;
  for (int i = 0; i < 32; ++i) {
    full_hash1.full_hash[i] = i % 2 ? 1 : 2;
    full_hash2.full_hash[i] = i % 2 ? 3 : 4;
  }

  add_chunk.append(full_hash1.full_hash, 32);
  add_chunk.append(full_hash2.full_hash, 32);

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(add_chunk.data(),
                                  static_cast<int>(add_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(1));

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_FULL_HASH);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_TRUE(entry->FullHashAt(0) == full_hash1);
  EXPECT_TRUE(entry->FullHashAt(1) == full_hash2);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing multiple add chunks. We'll use the same chunk as above, and add
// one more after it.
TEST(SafeBrowsingProtocolParsingTest, TestAddChunks) {
  std::string add_chunk("a:1:4:35\naaaax1111\0032222333344447777\00288889999"
                        "a:2:4:13\n5555\002ppppgggg");
  add_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(add_chunk.data(),
                                  static_cast<int>(add_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(2));
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(3));

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);


  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), static_cast<size_t>(1));

  EXPECT_EQ(chunks[1].hosts[0].host, 0x35353535);
  entry = chunks[1].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::ADD_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x70707070);
  EXPECT_EQ(entry->PrefixAt(1), 0x67676767);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing one add chunk where a hostkey spans several entries.
TEST(SafeBrowsingProtocolParsingTest, TestAddBigChunk) {
  std::string add_chunk("a:1:4:1050\naaaaX");
  add_chunk[add_chunk.size() - 1] |= 0xFF;
  for (int i = 0; i < 255; ++i)
    add_chunk.append(StringPrintf("%04d", i));

  add_chunk.append("aaaa");
  add_chunk.push_back(5);
  for (int i = 0; i < 5; ++i)
    add_chunk.append(StringPrintf("001%d", i));

  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(add_chunk.data(),
                                  static_cast<int>(add_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 1);

  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(1));

  const SBChunkHost& host = chunks[0].hosts[0];
  EXPECT_EQ(host.host, 0x61616161);
  EXPECT_EQ(host.entry->prefix_count(), 260);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing one sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestSubChunk) {
  std::string sub_chunk("s:9:4:59\naaaaxkkkk1111\003"
                        "zzzz2222zzzz3333zzzz4444"
                        "7777\002yyyy8888yyyy9999");
  sub_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(sub_chunk.data(),
                                  static_cast<int>(sub_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 9);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(3));

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::SUB_PREFIX);
  EXPECT_EQ(entry->chunk_id(), 0x6b6b6b6b);
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_EQ(entry->type(), SBEntry::SUB_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->ChunkIdAtPrefix(2), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_EQ(entry->type(), SBEntry::SUB_PREFIX);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x79797979);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x79797979);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing one sub chunk with full hashes.
TEST(SafeBrowsingProtocolParsingTest, TestSubFullChunk) {
  std::string sub_chunk("s:1:32:77\naaaa");
  sub_chunk.push_back(2);

  SBFullHash full_hash1, full_hash2;
  for (int i = 0; i < 32; ++i) {
    full_hash1.full_hash[i] = i % 2 ? 1 : 2;
    full_hash2.full_hash[i] = i % 2 ? 3 : 4;
  }

  sub_chunk.append("yyyy");
  sub_chunk.append(full_hash1.full_hash, 32);
  sub_chunk.append("zzzz");
  sub_chunk.append(full_hash2.full_hash, 32);

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;
  bool result = parser.ParseChunk(sub_chunk.data(),
                                  static_cast<int>(sub_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_FALSE(re_key);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(1));

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_EQ(entry->type(), SBEntry::SUB_FULL_HASH);
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x79797979);
  EXPECT_TRUE(entry->FullHashAt(0) == full_hash1);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x7a7a7a7a);
  EXPECT_TRUE(entry->FullHashAt(1) == full_hash2);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing the SafeBrowsing update response.
TEST(SafeBrowsingProtocolParsingTest, TestChunkDelete) {
  std::string add_del("n:1700\ni:phishy\nad:1-7,43-597,44444,99999\n"
                      "i:malware\nsd:21-27,42,171717\n");

  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool re_key = false;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(add_del.data(),
                                 static_cast<int>(add_del.length()), "",
                                 &next_query_sec, &re_key,
                                 &reset, &deletes, &urls));

  EXPECT_TRUE(urls.empty());
  EXPECT_FALSE(re_key);
  EXPECT_FALSE(reset);
  EXPECT_EQ(next_query_sec, 1700);
  EXPECT_EQ(deletes.size(), static_cast<size_t>(2));

  EXPECT_EQ(deletes[0].chunk_del.size(), static_cast<size_t>(4));
  EXPECT_TRUE(deletes[0].chunk_del[0] == ChunkRange(1, 7));
  EXPECT_TRUE(deletes[0].chunk_del[1] == ChunkRange(43, 597));
  EXPECT_TRUE(deletes[0].chunk_del[2] == ChunkRange(44444));
  EXPECT_TRUE(deletes[0].chunk_del[3] == ChunkRange(99999));

  EXPECT_EQ(deletes[1].chunk_del.size(), static_cast<size_t>(3));
  EXPECT_TRUE(deletes[1].chunk_del[0] == ChunkRange(21, 27));
  EXPECT_TRUE(deletes[1].chunk_del[1] == ChunkRange(42));
  EXPECT_TRUE(deletes[1].chunk_del[2] == ChunkRange(171717));

  // An update response with missing list name.

  next_query_sec = 0;
  deletes.clear();
  urls.clear();
  add_del = "n:1700\nad:1-7,43-597,44444,99999\ni:malware\nsd:4,21-27171717\n";
  EXPECT_FALSE(parser.ParseUpdate(add_del.data(),
                                  static_cast<int>(add_del.length()), "",
                                  &next_query_sec, &re_key,
                                  &reset, &deletes, &urls));
}

// Test parsing the SafeBrowsing update response.
TEST(SafeBrowsingProtocolParsingTest, TestRedirects) {
  std::string redirects("i:goog-malware-shavar\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_1\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_2\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_3\n"
    "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8641-8800:8641-8689,"
    "8691-8731,8733-8786\n");

  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool re_key = false;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(redirects.data(),
                                 static_cast<int>(redirects.length()), "",
                                 &next_query_sec, &re_key,
                                 &reset, &deletes, &urls));

  EXPECT_FALSE(re_key);
  EXPECT_FALSE(reset);
  EXPECT_EQ(urls.size(), static_cast<size_t>(4));
  EXPECT_EQ(urls[0].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_1");
  EXPECT_EQ(urls[1].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_2");
  EXPECT_EQ(urls[2].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_3");
  EXPECT_EQ(urls[3].url,
      "s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8641-8800:8641-8689,"
      "8691-8731,8733-8786");
  EXPECT_EQ(next_query_sec, 0);
  EXPECT_TRUE(deletes.empty());
}

TEST(SafeBrowsingProtocolParsingTest, TestRedirectsWithMac) {
  std::string redirects("i:goog-phish-shavar\n"
    "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6501-6505:6501-6505,"
    "pcY6iVeT9-CBQ3fdAF0rpnKjR1Y=\n"
    "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8001-8160:8001-8024,"
    "8026-8045,8048-8049,8051-8134,8136-8152,8155-8160,"
    "j6XXAEWnjYk9tVVLBSdQvIEq2Wg=\n");

  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool re_key = false;
  bool reset = false;
  const std::string key("58Lqn5WIP961x3zuLGo5Uw==");
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(redirects.data(),
                                 static_cast<int>(redirects.length()), key,
                                 &next_query_sec, &re_key,
                                 &reset, &deletes, &urls));

  EXPECT_FALSE(re_key);
  EXPECT_FALSE(reset);
  EXPECT_EQ(urls.size(), static_cast<size_t>(2));
  EXPECT_EQ(urls[0].url,
      "s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6501-6505:6501-6505");
  EXPECT_EQ(urls[0].mac, "pcY6iVeT9-CBQ3fdAF0rpnKjR1Y=");
  EXPECT_EQ(urls[1].url,
      "s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8001-8160:8001-8024,"
      "8026-8045,8048-8049,8051-8134,8136-8152,8155-8160");
  EXPECT_EQ(urls[1].mac, "j6XXAEWnjYk9tVVLBSdQvIEq2Wg=");
}

// Test parsing various SafeBrowsing protocol headers.
TEST(SafeBrowsingProtocolParsingTest, TestNextQueryTime) {
  std::string headers("n:1800\ni:goog-white-shavar\n");
  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool re_key = false;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(headers.data(),
                                 static_cast<int>(headers.length()), "",
                                 &next_query_sec, &re_key,
                                 &reset, &deletes, &urls));

  EXPECT_EQ(next_query_sec, 1800);
  EXPECT_FALSE(re_key);
  EXPECT_FALSE(reset);
  EXPECT_TRUE(deletes.empty());
  EXPECT_TRUE(urls.empty());
}

// Test parsing data from a GetHashRequest
TEST(SafeBrowsingProtocolParsingTest, TestGetHash) {
  std::string get_hash("goog-phish-shavar:19:96\n"
                       "00112233445566778899aabbccddeeff"
                       "00001111222233334444555566667777"
                       "ffffeeeeddddccccbbbbaaaa99998888");
  std::vector<SBFullHashResult> full_hashes;
  bool re_key = false;
  SafeBrowsingProtocolParser parser;
  parser.ParseGetHash(get_hash.data(),
                      static_cast<int>(get_hash.length()), "",
                      &re_key,
                      &full_hashes);

  EXPECT_FALSE(re_key);
  EXPECT_EQ(full_hashes.size(), static_cast<size_t>(3));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   "00112233445566778899aabbccddeeff",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[1].hash,
                   "00001111222233334444555566667777",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[2].hash,
                   "ffffeeeeddddccccbbbbaaaa99998888",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[2].list_name, "goog-phish-shavar");

  // Test multiple lists in the GetHash results.
  std::string get_hash2("goog-phish-shavar:19:32\n"
                        "00112233445566778899aabbccddeeff"
                        "goog-malware-shavar:19:64\n"
                        "cafebeefcafebeefdeaddeaddeaddead"
                        "zzzzyyyyxxxxwwwwvvvvuuuuttttssss");
  parser.ParseGetHash(get_hash2.data(),
                      static_cast<int>(get_hash2.length()), "",
                      &re_key,
                      &full_hashes);

  EXPECT_FALSE(re_key);
  EXPECT_EQ(full_hashes.size(), static_cast<size_t>(3));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   "00112233445566778899aabbccddeeff",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[1].hash,
                   "cafebeefcafebeefdeaddeaddeaddead",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-malware-shavar");
  EXPECT_EQ(memcmp(&full_hashes[2].hash,
                   "zzzzyyyyxxxxwwwwvvvvuuuuttttssss",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[2].list_name, "goog-malware-shavar");
}

TEST(SafeBrowsingProtocolParsingTest, TestGetHashWithMac) {
  const unsigned char get_hash[] = {
    0x32, 0x56, 0x74, 0x6f, 0x6b, 0x36, 0x64, 0x41,
    0x51, 0x72, 0x65, 0x51, 0x62, 0x38, 0x51, 0x68,
    0x59, 0x45, 0x57, 0x51, 0x57, 0x4d, 0x52, 0x65,
    0x42, 0x63, 0x41, 0x3d, 0x0a, 0x67, 0x6f, 0x6f,
    0x67, 0x2d, 0x70, 0x68, 0x69, 0x73, 0x68, 0x2d,
    0x73, 0x68, 0x61, 0x76, 0x61, 0x72, 0x3a, 0x36,
    0x31, 0x36, 0x39, 0x3a, 0x33, 0x32, 0x0a, 0x17,
    0x7f, 0x03, 0x42, 0x28, 0x1c, 0x31, 0xb9, 0x0b,
    0x1c, 0x7b, 0x9d, 0xaf, 0x7b, 0x43, 0x99, 0x10,
    0xc1, 0xab, 0xe3, 0x1b, 0x35, 0x80, 0x38, 0x96,
    0xf9, 0x44, 0x4f, 0x28, 0xb4, 0xeb, 0x45
  };

  const unsigned char hash_result [] = {
    0x17, 0x7f, 0x03, 0x42, 0x28, 0x1c, 0x31, 0xb9,
    0x0b, 0x1c, 0x7b, 0x9d, 0xaf, 0x7b, 0x43, 0x99,
    0x10, 0xc1, 0xab, 0xe3, 0x1b, 0x35, 0x80, 0x38,
    0x96, 0xf9, 0x44, 0x4f, 0x28, 0xb4, 0xeb, 0x45
  };

  const std::string key = "58Lqn5WIP961x3zuLGo5Uw==";
  std::vector<SBFullHashResult> full_hashes;
  bool re_key = false;
  SafeBrowsingProtocolParser parser;
  EXPECT_TRUE(parser.ParseGetHash(reinterpret_cast<const char*>(get_hash),
                                  sizeof(get_hash),
                                  key,
                                  &re_key,
                                  &full_hashes));
  EXPECT_FALSE(re_key);
  EXPECT_EQ(full_hashes.size(), static_cast<size_t>(1));
  EXPECT_EQ(memcmp(hash_result, &full_hashes[0].hash, sizeof(SBFullHash)), 0);
}

TEST(SafeBrowsingProtocolParsingTest, TestGetHashWithUnknownList) {
  std::string hash_response = "goog-phish-shavar:1:32\n"
                              "12345678901234567890123456789012"
                              "googpub-phish-shavar:19:32\n"
                              "09876543210987654321098765432109";
  bool re_key = false;
  std::string key = "";
  std::vector<SBFullHashResult> full_hashes;
  SafeBrowsingProtocolParser parser;
  EXPECT_TRUE(parser.ParseGetHash(hash_response.data(),
                                  hash_response.size(),
                                  key,
                                  &re_key,
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 1);
  EXPECT_EQ(memcmp("12345678901234567890123456789012", 
                   &full_hashes[0].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(full_hashes[0].add_chunk_id, 1);

  hash_response += "goog-malware-shavar:7:32\n"
                   "abcdefghijklmnopqrstuvwxyz123457";
  full_hashes.clear();
  EXPECT_TRUE(parser.ParseGetHash(hash_response.data(),
                                  hash_response.size(),
                                  key,
                                  &re_key,
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 2);
  EXPECT_EQ(memcmp("12345678901234567890123456789012", 
                   &full_hashes[0].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(full_hashes[0].add_chunk_id, 1);
  EXPECT_EQ(memcmp("abcdefghijklmnopqrstuvwxyz123457", 
                   &full_hashes[1].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-malware-shavar");
  EXPECT_EQ(full_hashes[1].add_chunk_id, 7);
}

TEST(SafeBrowsingProtocolParsingTest, TestFormatHash) {
  SafeBrowsingProtocolParser parser;
  std::vector<SBPrefix> prefixes;
  std::string get_hash;

  prefixes.push_back(0x34333231);
  prefixes.push_back(0x64636261);
  prefixes.push_back(0x73727170);

  parser.FormatGetHash(prefixes, &get_hash);
  EXPECT_EQ(get_hash, "4:12\n1234abcdpqrs");
}

TEST(SafeBrowsingProtocolParsingTest, TestGetKey) {
  SafeBrowsingProtocolParser parser;
  std::string key_response("clientkey:10:0123456789\n"
                           "wrappedkey:20:abcdefghijklmnopqrst\n");

  std::string client_key, wrapped_key;
  EXPECT_TRUE(parser.ParseNewKey(key_response.data(),
                                 static_cast<int>(key_response.length()),
                                 &client_key,
                                 &wrapped_key));

  EXPECT_EQ(client_key, "0123456789");
  EXPECT_EQ(wrapped_key, "abcdefghijklmnopqrst");
}

TEST(SafeBrowsingProtocolParsingTest, TestReKey) {
  SafeBrowsingProtocolParser parser;
  std::string update("n:1800\ni:phishy\ne:pleaserekey\n");

  bool re_key = false;
  bool reset = false;
  int next_update = -1;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(update.data(),
                                 static_cast<int>(update.size()), "",
                                 &next_update, &re_key,
                                 &reset, &deletes, &urls));
  EXPECT_TRUE(re_key);
}

TEST(SafeBrowsingProtocolParsingTest, TestReset) {
  SafeBrowsingProtocolParser parser;
  std::string update("n:1800\ni:phishy\nr:pleasereset\n");

  bool re_key = false;
  bool reset = false;
  int next_update = -1;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(update.data(),
                                 static_cast<int>(update.size()), "",
                                 &next_update, &re_key,
                                 &reset, &deletes, &urls));
  EXPECT_TRUE(reset);
}

// The SafeBrowsing service will occasionally send zero length chunks so that
// client requests will have longer contiguous chunk number ranges, and thus
// reduce the request size.
TEST(SafeBrowsingProtocolParsingTest, TestZeroSizeAddChunk) {
  std::string add_chunk("a:1:4:0\n");
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;

  bool result = parser.ParseChunk(add_chunk.data(),
                                  static_cast<int>(add_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(0));

  safe_browsing_util::FreeChunks(&chunks);

  // Now test a zero size chunk in between normal chunks.
  chunks.clear();
  std::string add_chunks("a:1:4:18\n1234\001abcd5678\001wxyz"
                         "a:2:4:0\n"
                         "a:3:4:9\ncafe\001beef");
  result = parser.ParseChunk(add_chunks.data(),
                             static_cast<int>(add_chunks.length()),
                             "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(3));

  // See that each chunk has the right content.
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(chunks[0].hosts[0].host, 0x34333231);
  EXPECT_EQ(chunks[0].hosts[0].entry->PrefixAt(0), 0x64636261);
  EXPECT_EQ(chunks[0].hosts[1].host, 0x38373635);
  EXPECT_EQ(chunks[0].hosts[1].entry->PrefixAt(0), 0x7a797877);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), static_cast<size_t>(0));
  
  EXPECT_EQ(chunks[2].chunk_number, 3);
  EXPECT_EQ(chunks[2].hosts.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[2].hosts[0].host, 0x65666163);
  EXPECT_EQ(chunks[2].hosts[0].entry->PrefixAt(0), 0x66656562);

  safe_browsing_util::FreeChunks(&chunks);
}

// Test parsing a zero sized sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestZeroSizeSubChunk) {
  std::string sub_chunk("s:9:4:0\n");
  SafeBrowsingProtocolParser parser;
  bool re_key = false;
  std::deque<SBChunk> chunks;

  bool result = parser.ParseChunk(sub_chunk.data(),
                                  static_cast<int>(sub_chunk.length()),
                                  "", "", &re_key, &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].chunk_number, 9);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(0));

  safe_browsing_util::FreeChunks(&chunks);
  chunks.clear();

  // Test parsing a zero sized sub chunk mixed in with content carrying chunks.
  std::string sub_chunks("s:1:4:9\nabcdxwxyz"
                         "s:2:4:0\n"
                         "s:3:4:26\nefgh\0011234pqrscafe\0015678lmno");
  sub_chunks[12] = '\0';

  result = parser.ParseChunk(sub_chunks.data(),
                             static_cast<int>(sub_chunks.length()),
                             "", "", &re_key, &chunks);
  EXPECT_TRUE(result);

  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), static_cast<size_t>(1));
  EXPECT_EQ(chunks[0].hosts[0].host, 0x64636261);
  EXPECT_EQ(chunks[0].hosts[0].entry->prefix_count(), 0);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), static_cast<size_t>(0));

  EXPECT_EQ(chunks[2].chunk_number, 3);
  EXPECT_EQ(chunks[2].hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(chunks[2].hosts[0].host, 0x68676665);
  EXPECT_EQ(chunks[2].hosts[0].entry->prefix_count(), 1);
  EXPECT_EQ(chunks[2].hosts[0].entry->PrefixAt(0), 0x73727170);
  EXPECT_EQ(chunks[2].hosts[0].entry->ChunkIdAtPrefix(0), 0x31323334);
  EXPECT_EQ(chunks[2].hosts[1].host, 0x65666163);
  EXPECT_EQ(chunks[2].hosts[1].entry->prefix_count(), 1);
  EXPECT_EQ(chunks[2].hosts[1].entry->PrefixAt(0), 0x6f6e6d6c);
  EXPECT_EQ(chunks[2].hosts[1].entry->ChunkIdAtPrefix(0), 0x35363738);

  safe_browsing_util::FreeChunks(&chunks);
}

TEST(SafeBrowsingProtocolParsingTest, TestVerifyUpdateMac) {
  SafeBrowsingProtocolParser parser;

  const std::string update =
      "m:XIU0LiQhAPJq6dynXwHbygjS5tw=\n"
      "n:1895\n"
      "i:goog-phish-shavar\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6501-6505:6501-6505,pcY6iVeT9-CBQ3fdAF0rpnKjR1Y=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6506-6510:6506-6510,SDBrYC3rX3KEPe72LOypnP6QYac=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6511-6520:6511-6520,9UQo-e7OkcsXT2wFWTAhOuWOsUs=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6521-6560:6521-6560,qVNw6JIpR1q6PIXST7J4LJ9n3Zg=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6561-6720:6561-6720,7OiJvCbiwvpzPITW-hQohY5NHuc=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6721-6880:6721-6880,oBS3svhoi9deIa0sWZ_gnD0ujj8=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_6881-7040:6881-7040,a0r8Xit4VvH39xgyQHZTPczKBIE=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_s_7041-7200:7041-7163,q538LChutGknBw55s6kcE2wTcvU=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8001-8160:8001-8024,8026-8045,8048-8049,8051-8134,8136-8152,8155-8160,j6XXAEWnjYk9tVVLBSdQvIEq2Wg=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8161-8320:8161-8215,8217-8222,8224-8320,YaNfiqdQOt-uLCLWVLj46AZpAjQ=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8321-8480:8321-8391,8393-8399,8402,8404-8419,8421-8425,8427,8431-8433,8435-8439,8441-8443,8445-8446,8448-8480,ALj31GQMwGiIeU3bM2ZYKITfU-U=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8481-8640:8481-8500,8502-8508,8510-8511,8513-8517,8519-8525,8527-8531,8533,8536-8539,8541-8576,8578-8638,8640,TlQYRmS_kZ5PBAUIUyNQDq0Jprs=\n"
      "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8641-8800:8641-8689,8691-8731,8733-8786,x1Qf7hdNrO8b6yym03ZzNydDS1o=\n";

  bool re_key = false;
  bool reset = false;
  int next_update = -1;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  const std::string key("58Lqn5WIP961x3zuLGo5Uw==");
  EXPECT_TRUE(parser.ParseUpdate(update.data(),
                                 static_cast<int>(update.size()), key,
                                 &next_update, &re_key,
                                 &reset, &deletes, &urls));
  EXPECT_FALSE(re_key);
  EXPECT_EQ(next_update, 1895);
}

TEST(SafeBrowsingProtocolParsingTest, TestVerifyChunkMac) {
  SafeBrowsingProtocolParser parser;

  const unsigned char chunk[] = {
    0x73, 0x3a, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x34,
    0x3a, 0x32, 0x32, 0x0a, 0x2f, 0x4f, 0x89, 0x7a,
    0x01, 0x00, 0x00, 0x0a, 0x59, 0xc8, 0x71, 0xdf,
    0x9d, 0x29, 0x0c, 0xba, 0xd7, 0x00, 0x00, 0x00,
    0x0a, 0x59
  };

  bool re_key = false;
  std::deque<SBChunk> chunks;
  const std::string key("v_aDSz6jI92WeHCOoZ07QA==");
  const std::string mac("W9Xp2fUcQ9V66If6Cvsrstpa4Kk=");

  EXPECT_TRUE(parser.ParseChunk(reinterpret_cast<const char*>(chunk),
                                sizeof(chunk), key, mac,
                                &re_key, &chunks));
  EXPECT_FALSE(re_key);

  safe_browsing_util::FreeChunks(&chunks);
}
