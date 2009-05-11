// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test performs a series false positive checks using a list of URLs
// against a known set of SafeBrowsing data.
//
// It uses a normal SafeBrowsing database to create a bloom filter where it
// looks up all the URLs in the url file. A URL that has a prefix found in the
// bloom filter and found in the database is considered a hit: a valid lookup
// that will result in a gethash request. A URL that has a prefix found in the
// bloom filter but not in the database is a miss: a false positive lookup that
// will result in an unnecessary gethash request.
//
// By varying the size of the bloom filter and using a constant set of
// SafeBrowsing data, we can check a known set of URLs against the filter and
// determine the false positive rate.
//
// False positive calculation usage:
//   $ ./perf_tests.exe --gtest_filter=SafeBrowsingBloomFilter.FalsePositives
//                      --filter-start=<integer>
//                      --filter-steps=<integer>
//                      --filter-verbose
//
//  --filter-start:   The filter multiplier to begin with. This represents the
//                    number of bits per prefix of memory to use in the filter.
//                    The default value is identical to the current SafeBrowsing
//                    database value.
//  --filter-steps:   The number of iterations to run, with each iteration
//                    increasing the filter multiplier by 1. The default value
//                    is 1.
//  --filter-verbose: Used to print out the hit / miss results per URL.
//  --filter-csv:     The URL file contains information about the number of
//                    unique views (the popularity) of each URL. See the format
//                    description below.
//
//
// Hash compute time usage:
//   $ ./perf_tests.exe --gtest_filter=SafeBrowsingBloomFilter.HashTime
//                      --filter-num-checks=<integer>
//
//  --filter-num-checks: The number of hash look ups to perform on the bloom
//                       filter. The default is 10 million.
//
// Data files:
//    chrome/test/data/safe_browsing/filter/database
//    chrome/test/data/safe_browsing/filter/urls
//
// database: A normal SafeBrowsing database.
// urls:     A text file containing a list of URLs, one per line. If the option
//           --filter-csv is specified, the format of each line in the file is
//           <url>,<weight> where weight is an integer indicating the number of
//           unique views for the URL.

#include <fstream>
#include <iostream>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

// Ensures the SafeBrowsing database is closed properly.
class ScopedPerfDatabase {
 public:
  explicit ScopedPerfDatabase(sqlite3* db) : db_(db) {}
  ~ScopedPerfDatabase() {
    sqlite3_close(db_);
  }

 private:
  sqlite3* db_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPerfDatabase);
};

// Command line flags.
const wchar_t kFilterVerbose[] = L"filter-verbose";
const wchar_t kFilterStart[] = L"filter-start";
const wchar_t kFilterSteps[] = L"filter-steps";
const wchar_t kFilterCsv[] = L"filter-csv";
const wchar_t kFilterNumChecks[] = L"filter-num-checks";

// Number of hash checks to make during performance testing.
static const int kNumHashChecks = 10000000;

// Returns the path to the data used in this test, relative to the top of the
// source directory.
FilePath GetFullDataPath() {
  FilePath full_path;
  CHECK(PathService::Get(chrome::DIR_TEST_DATA, &full_path));
  full_path = full_path.Append(FILE_PATH_LITERAL("safe_browsing"));
  full_path = full_path.Append(FILE_PATH_LITERAL("filter"));
  CHECK(file_util::PathExists(full_path));
  return full_path;
}

// Constructs a bloom filter of the appropriate size from the provided prefixes.
void BuildBloomFilter(int size_multiplier,
                      const std::vector<SBPrefix>& prefixes,
                      BloomFilter** bloom_filter) {
  // Create a BloomFilter with the specified size.
  const int key_count = std::max(static_cast<int>(prefixes.size()),
                                 BloomFilter::kBloomFilterMinSize);
  const int filter_size = key_count * size_multiplier;
  *bloom_filter = new BloomFilter(filter_size);

  // Add the prefixes to it.
  for (size_t i = 0; i < prefixes.size(); ++i)
    (*bloom_filter)->Insert(prefixes[i]);

  std::cout << "Bloom filter with prefixes: " << prefixes.size()
            << ", multiplier: " << size_multiplier
            << ", size (bytes): " << (*bloom_filter)->size()
            << std::endl;
}

// Reads the set of add prefixes contained in a SafeBrowsing database into a
// sorted array suitable for fast searching. This takes significantly less time
// to look up a given prefix than performing SQL queries.
bool ReadDatabase(const FilePath& path, std::vector<SBPrefix>* prefixes) {
  FilePath database_file = path.Append(FILE_PATH_LITERAL("database"));
  sqlite3* db = NULL;
  if (OpenSqliteDb(database_file, &db) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  ScopedPerfDatabase database(db);
  scoped_ptr<SqliteStatementCache> sql_cache(new SqliteStatementCache(db));

  // Get the number of items in the add_prefix table.
  std::string sql = "SELECT COUNT(*) FROM add_prefix";
  SQLITE_UNIQUE_STATEMENT(count_statement, *sql_cache, sql.c_str());
  if (!count_statement.is_valid())
    return false;

  if (count_statement->step() != SQLITE_ROW)
    return false;

  const int count = count_statement->column_int(0);

  // Load them into a prefix vector and sort
  prefixes->reserve(count);
  sql = "SELECT prefix FROM add_prefix";
  SQLITE_UNIQUE_STATEMENT(prefix_statement, *sql_cache, sql.c_str());
  if (!prefix_statement.is_valid())
    return false;

  while (prefix_statement->step() == SQLITE_ROW)
    prefixes->push_back(prefix_statement->column_int(0));

  DCHECK(static_cast<int>(prefixes->size()) == count);
  sort(prefixes->begin(), prefixes->end());

  return true;
}

// Generates all legal SafeBrowsing prefixes for the specified URL, and returns
// the set of Prefixes that exist in the bloom filter. The function returns the
// number of host + path combinations checked.
int GeneratePrefixHits(const std::string url,
                       BloomFilter* bloom_filter,
                       std::vector<SBPrefix>* prefixes) {
  GURL url_check(url);
  std::vector<std::string> hosts;
  if (url_check.HostIsIPAddress()) {
    hosts.push_back(url_check.host());
  } else {
    safe_browsing_util::GenerateHostsToCheck(url_check, &hosts);
  }

  std::vector<std::string> paths;
  safe_browsing_util::GeneratePathsToCheck(url_check, &paths);

  for (size_t i = 0; i < hosts.size(); ++i) {
    for (size_t j = 0; j < paths.size(); ++j) {
      SBPrefix prefix;
      base::SHA256HashString(hosts[i] + paths[j], &prefix, sizeof(prefix));
      if (bloom_filter->Exists(prefix))
        prefixes->push_back(prefix);
    }
  }

  return hosts.size() * paths.size();
}

// Binary search of sorted prefixes.
bool IsPrefixInDatabase(SBPrefix prefix,
                        const std::vector<SBPrefix>& prefixes) {
  if (prefixes.empty())
    return false;

  int low = 0;
  int high = prefixes.size() - 1;
  while (low <= high) {
    int mid = ((unsigned int)low + (unsigned int)high) >> 1;
    SBPrefix prefix_mid = prefixes[mid];
    if (prefix_mid == prefix)
      return true;
    if (prefix_mid < prefix)
      low = mid + 1;
    else
      high = mid - 1;
  }

  return false;
}

// Construct a bloom filter with the given prefixes and multiplier, and test the
// false positive rate (misses) against a URL list.
void CalculateBloomFilterFalsePositives(
    int size_multiplier,
    const FilePath& data_dir,
    const std::vector<SBPrefix>& prefix_list) {
  BloomFilter* bloom_filter = NULL;
  BuildBloomFilter(size_multiplier, prefix_list, &bloom_filter);
  scoped_refptr<BloomFilter> scoped_filter(bloom_filter);

  // Read in data file line at a time.
  FilePath url_file = data_dir.Append(FILE_PATH_LITERAL("urls"));
  std::ifstream url_stream(WideToASCII(url_file.ToWStringHack()).c_str());

  // Keep track of stats
  int hits = 0;
  int misses = 0;
  int weighted_hits = 0;
  int weighted_misses = 0;
  int url_count = 0;
  int prefix_count = 0;

  // Print out volumes of data (per URL hit and miss information).
  bool verbose = CommandLine::ForCurrentProcess()->HasSwitch(kFilterVerbose);
  bool use_weights = CommandLine::ForCurrentProcess()->HasSwitch(kFilterCsv);

  std::string url;
  while (std::getline(url_stream, url)) {
    ++url_count;

    // Handle a format that contains URLs weighted by unique views.
    int weight = 1;
    if (use_weights) {
      std::string::size_type pos = url.find_last_of(",");
      if (pos != std::string::npos) {
        weight = StringToInt(std::string(url, pos + 1));
        url = url.substr(0, pos);
      }
    }

    // See if the URL is in the bloom filter.
    std::vector<SBPrefix> prefixes;
    prefix_count += GeneratePrefixHits(url, bloom_filter, &prefixes);

    // See if the prefix is actually in the database (in-memory prefix list).
    for (size_t i = 0; i < prefixes.size(); ++i) {
      if (IsPrefixInDatabase(prefixes[i], prefix_list)) {
        ++hits;
        weighted_hits += weight;
        if (verbose) {
          std::cout << "Hit for URL: " << url
                    << " (prefix = "   << prefixes[i] << ")"
                    << std::endl;
        }
      } else {
        ++misses;
        weighted_misses += weight;
        if (verbose) {
          std::cout << "Miss for URL: " << url
                    << " (prefix = "    << prefixes[i] << ")"
                    << std::endl;
        }
      }
    }
  }

  // Print out the results for this test.
  std::cout << "URLs checked: "      << url_count
            << ", prefix compares: " << prefix_count
            << ", hits: "            << hits
            << ", misses: "          << misses;
  if (use_weights) {
    std::cout << ", weighted hits: "   << weighted_hits
              << ", weighted misses: " << weighted_misses;
  }
  std::cout << std::endl;
}

}  // namespace

// This test can take several minutes to perform its calculations, so it should
// be disabled until you need to run it.
TEST(SafeBrowsingBloomFilter, FalsePositives) {
  std::vector<SBPrefix> prefix_list;
  FilePath data_dir = GetFullDataPath();
  ASSERT_TRUE(ReadDatabase(data_dir, &prefix_list));

  int start = BloomFilter::kBloomFilterSizeRatio;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kFilterStart)) {
    start = StringToInt(
        CommandLine::ForCurrentProcess()->GetSwitchValue(kFilterStart));
  }

  int steps = 1;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kFilterSteps)) {
    steps = StringToInt(
        CommandLine::ForCurrentProcess()->GetSwitchValue(kFilterSteps));
  }

  int stop = start + steps;

  for (int multiplier = start; multiplier < stop; ++multiplier)
    CalculateBloomFilterFalsePositives(multiplier, data_dir, prefix_list);
}

// Computes the time required for performing a number of look ups in a bloom
// filter. This is useful for measuring the performance of new hash functions.
TEST(SafeBrowsingBloomFilter, HashTime) {
  // Read the data from the database.
  std::vector<SBPrefix> prefix_list;
  FilePath data_dir = GetFullDataPath();
  ASSERT_TRUE(ReadDatabase(data_dir, &prefix_list));

  int num_checks = kNumHashChecks;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kFilterNumChecks)) {
    num_checks = StringToInt(
      CommandLine::ForCurrentProcess()->GetSwitchValue(kFilterNumChecks));
  }

  // Populate the bloom filter and measure the time.
  BloomFilter* bloom_filter = NULL;
  Time populate_before = Time::Now();
  BuildBloomFilter(BloomFilter::kBloomFilterSizeRatio,
                   prefix_list, &bloom_filter);
  TimeDelta populate = Time::Now() - populate_before;

  // Check a large number of random prefixes against the filter.
  int hits = 0;
  Time check_before = Time::Now();
  for (int i = 0; i < num_checks; ++i) {
    uint32 prefix = static_cast<uint32>(base::RandUint64());
    if (bloom_filter->Exists(prefix))
      ++hits;
  }
  TimeDelta check = Time::Now() - check_before;

  int64 time_per_insert = populate.InMicroseconds() /
                          static_cast<int>(prefix_list.size());
  int64 time_per_check = check.InMicroseconds() / num_checks;

  std::cout << "Time results for checks: " << num_checks
            << ", prefixes: "              << prefix_list.size()
            << ", populate time (ms): "    << populate.InMilliseconds()
            << ", check time (ms): "       << check.InMilliseconds()
            << ", hits: "                  << hits
            << ", per-populate (us): "     << time_per_insert
            << ", per-check (us): "        << time_per_check
            << std::endl;
}

