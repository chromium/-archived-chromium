// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include <limits>
#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test_file_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Base class for a safebrowsing database.  Derived classes can implement
// different types of tables to compare performance characteristics.
class Database {
 public:
  Database() : db_(NULL) {
  }

  ~Database() {
    if (db_) {
      sqlite3_close(db_);
      db_ = NULL;
    }
  }

  bool Init(const FilePath& name, bool create) {
    // get an empty file for the test DB
    FilePath filename;
    PathService::Get(base::DIR_TEMP, &filename);
    filename = filename.Append(name);

    if (create) {
      file_util::Delete(filename, false);
    } else {
      DLOG(INFO) << "evicting " << name.value() << " ...";
      file_util::EvictFileFromSystemCache(filename);
      DLOG(INFO) << "... evicted";
    }

    const std::string sqlite_path = WideToUTF8(filename.ToWStringHack());
    if (sqlite3_open(sqlite_path.c_str(), &db_) != SQLITE_OK)
      return false;

    statement_cache_.set_db(db_);

    if (!create)
      return true;

    return CreateTable();
  }

  virtual bool CreateTable() = 0;
  virtual bool Add(int host_key, int* prefixes, int count) = 0;
  virtual bool Read(int host_key, int* prefixes, int size, int* count) = 0;
  virtual int Count() = 0;
  virtual std::string GetDBSuffix() = 0;

  sqlite3* db() { return db_; }

 protected:
  // The database connection.
  sqlite3* db_;

  // Cache of compiled statements for our database.
  SqliteStatementCache statement_cache_;
};

class SimpleDatabase : public Database {
 public:
  virtual bool CreateTable() {
    if (DoesSqliteTableExist(db_, "hosts"))
      return false;

    return sqlite3_exec(db_, "CREATE TABLE hosts ("
        "host INTEGER,"
        "prefixes BLOB)",
        NULL, NULL, NULL) == SQLITE_OK;
  }

  virtual bool Add(int host_key, int* prefixes, int count) {
    SQLITE_UNIQUE_STATEMENT(statement, statement_cache_,
        "INSERT OR REPLACE INTO hosts"
        "(host,prefixes)"
        "VALUES (?,?)");
    if (!statement.is_valid())
      return false;

    statement->bind_int(0, host_key);
    statement->bind_blob(1, prefixes, count*sizeof(int));
    return statement->step() == SQLITE_DONE;
  }

  virtual bool Read(int host_key, int* prefixes, int size, int* count) {
    SQLITE_UNIQUE_STATEMENT(statement, statement_cache_,
        "SELECT host, prefixes FROM hosts WHERE host=?");
    if (!statement.is_valid())
      return false;

    statement->bind_int(0, host_key);

    int rv = statement->step();
    if (rv == SQLITE_DONE) {
      // no hostkey found, not an error
      *count = -1;
      return true;
    }

    if (rv != SQLITE_ROW)
      return false;

    *count = statement->column_bytes(1);
    if (*count > size)
      return false;

    memcpy(prefixes, statement->column_blob(0), *count);
    return true;
  }

  int Count() {
    SQLITE_UNIQUE_STATEMENT(statement, statement_cache_,
        "SELECT COUNT(*) FROM hosts");
    if (!statement.is_valid()) {
      EXPECT_TRUE(false);
      return -1;
    }

    if (statement->step() != SQLITE_ROW) {
      EXPECT_TRUE(false);
      return -1;
    }

    return statement->column_int(0);
  }

  std::string GetDBSuffix() {
    return "Simple";
  }
};

class IndexedDatabase : public SimpleDatabase {
 public:
  virtual bool CreateTable() {
    return sqlite3_exec(db_, "CREATE TABLE hosts ("
        "host INTEGER PRIMARY KEY,"
        "prefixes BLOB)",
        NULL, NULL, NULL) == SQLITE_OK;
  }

  std::string GetDBSuffix() {
    return "Indexed";
  }
};

class IndexedWithIDDatabase : public SimpleDatabase {
 public:
  virtual bool CreateTable() {
    return sqlite3_exec(db_, "CREATE TABLE hosts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "host INTEGER UNIQUE,"
        "prefixes BLOB)",
        NULL, NULL, NULL) == SQLITE_OK;
  }

  virtual bool Add(int host_key, int* prefixes, int count) {
    SQLITE_UNIQUE_STATEMENT(statement, statement_cache_,
        "INSERT OR REPLACE INTO hosts"
        "(id,host,prefixes)"
        "VALUES (NULL,?,?)");
    if (!statement.is_valid())
      return false;

    statement->bind_int(0, host_key);
    statement->bind_blob(1, prefixes, count * sizeof(int));
    return statement->step() == SQLITE_DONE;
  }

  std::string GetDBSuffix() {
    return "IndexedWithID";
  }
};

}  // namespace

class SafeBrowsing: public testing::Test {
 protected:
  // Get the test parameters from the test case's name.
  virtual void SetUp() {
    logging::InitLogging(
        NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE);

    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = test_info->name();

    TestType type;
    if (test_name.find("Write") != std::string::npos) {
      type = WRITE;
    } else if (test_name.find("Read") != std::string::npos) {
      type = READ;
    } else {
      type = COUNT;
    }

    if (test_name.find("IndexedWithID") != std::string::npos) {
      db_ = new IndexedWithIDDatabase();
    } else if (test_name.find("Indexed")  != std::string::npos) {
      db_ = new IndexedDatabase();
    } else {
      db_ = new SimpleDatabase();
    }


    char multiplier_letter = test_name[test_name.size() - 1];
    int multiplier = 0;
    if (multiplier_letter == 'K') {
      multiplier = 1000;
    } else if (multiplier_letter == 'M') {
      multiplier = 1000000;
    } else {
      NOTREACHED();
    }

    size_t index = test_name.size() - 1;
    while (index != 0 && test_name[index] != '_')
      index--;

    DCHECK(index);
    const char* count_start = test_name.c_str() + ++index;
    int count = atoi(count_start);
    int size = count * multiplier;

    db_name_ = StringPrintf("TestSafeBrowsing");
    db_name_.append(count_start);
    db_name_.append(db_->GetDBSuffix());

    FilePath path = FilePath::FromWStringHack(ASCIIToWide(db_name_));
    ASSERT_TRUE(db_->Init(path, type == WRITE));

    if (type == WRITE) {
      WriteEntries(size);
    } else if (type == READ) {
      ReadEntries(100);
    } else {
      CountEntries();
    }
  }

  virtual void TearDown() {
    delete db_;
  }

  // This writes the given number of entries to the database.
  void WriteEntries(int count) {
    int prefixes[4];

    SQLTransaction transaction(db_->db());
    transaction.Begin();

    for (int i = 0; i < count; i++) {
      int hostkey = base::RandInt(std::numeric_limits<int>::min(),
                                  std::numeric_limits<int>::max());
      ASSERT_TRUE(db_->Add(hostkey, prefixes, 1));
    }

    transaction.Commit();
  }

  // Read the given number of entries from the database.
  void ReadEntries(int count) {
    int prefixes[4];

    int64 total_ms = 0;

    for (int i = 0; i < count; ++i) {
      int key = base::RandInt(std::numeric_limits<int>::min(),
                              std::numeric_limits<int>::max());

      PerfTimer timer;

      int read;
      ASSERT_TRUE(db_->Read(key, prefixes, sizeof(prefixes), &read));

      int64 time_ms = timer.Elapsed().InMilliseconds();
      total_ms += time_ms;
      DLOG(INFO) << "Read in " << time_ms << " ms.";
    }

    DLOG(INFO) << db_name_ << " read " << count << " entries in average of " <<
        total_ms/count << " ms.";
  }

  // Counts how many entries are in the database, which effectively does a full
  // table scan.
  void CountEntries() {
    PerfTimer timer;

    int count = db_->Count();

    DLOG(INFO) << db_name_ << " counted " << count << " entries in " <<
        timer.Elapsed().InMilliseconds() << " ms";
  }

  enum TestType {
    WRITE,
    READ,
    COUNT,
  };

 private:

  Database* db_;
  std::string db_name_;
};

TEST_F(SafeBrowsing, DISABLED_Write_100K) {
}

TEST_F(SafeBrowsing, DISABLED_Read_100K) {
}

TEST_F(SafeBrowsing, DISABLED_WriteIndexed_100K) {
}

TEST_F(SafeBrowsing, DISABLED_ReadIndexed_100K) {
}

TEST_F(SafeBrowsing, DISABLED_WriteIndexed_250K) {
}

TEST_F(SafeBrowsing, DISABLED_ReadIndexed_250K) {
}

TEST_F(SafeBrowsing, DISABLED_WriteIndexed_500K) {
}

TEST_F(SafeBrowsing, DISABLED_ReadIndexed_500K) {
}

TEST_F(SafeBrowsing, DISABLED_WriteIndexedWithID_250K) {
}

TEST_F(SafeBrowsing, DISABLED_ReadIndexedWithID_250K) {
}

TEST_F(SafeBrowsing, DISABLED_WriteIndexedWithID_500K) {
}

TEST_F(SafeBrowsing, DISABLED_ReadIndexedWithID_500K) {
}

TEST_F(SafeBrowsing, DISABLED_CountIndexed_250K) {
}

TEST_F(SafeBrowsing, DISABLED_CountIndexed_500K) {
}

TEST_F(SafeBrowsing, DISABLED_CountIndexedWithID_250K) {
}

TEST_F(SafeBrowsing, DISABLED_CountIndexedWithID_500K) {
}


class SafeBrowsingDatabaseTest {
 public:
  SafeBrowsingDatabaseTest(const FilePath& filename) {
    logging::InitLogging(
        NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE);

    FilePath tmp_path;
    PathService::Get(base::DIR_TEMP, &tmp_path);
    path_ = tmp_path.Append(filename);
  }

  void Create(int size) {
    file_util::Delete(path_, false);

    scoped_ptr<SafeBrowsingDatabase> database(SafeBrowsingDatabase::Create());
    database->SetSynchronous();
    EXPECT_TRUE(database->Init(path_, NULL));

    int chunk_id = 0;
    int total_host_keys = size;
    int host_keys_per_chunk = 100;

    std::deque<SBChunk>* chunks = new std::deque<SBChunk>;

    for (int i = 0; i < total_host_keys / host_keys_per_chunk; ++i) {
      chunks->push_back(SBChunk());
      chunks->back().chunk_number = ++chunk_id;

      for (int j = 0; j < host_keys_per_chunk; ++j) {
        SBChunkHost host;
        host.host = base::RandInt(std::numeric_limits<int>::min(),
                                  std::numeric_limits<int>::max());
        host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
        host.entry->SetPrefixAt(0, 0x2425525);
        host.entry->SetPrefixAt(1, 0x1536366);

        chunks->back().hosts.push_back(host);
      }
    }

    database->InsertChunks("goog-malware", chunks);
  }

  void Read(bool use_bloom_filter) {
    int keys_to_read = 500;
    file_util::EvictFileFromSystemCache(path_);

    scoped_ptr<SafeBrowsingDatabase> database(SafeBrowsingDatabase::Create());
    database->SetSynchronous();
    EXPECT_TRUE(database->Init(path_, NULL));

    PerfTimer total_timer;
    int64 db_ms = 0;
    int keys_from_db = 0;
    for (int i = 0; i < keys_to_read; ++i) {
      int key = base::RandInt(std::numeric_limits<int>::min(),
                              std::numeric_limits<int>::max());

      std::string url = StringPrintf("http://www.%d.com/blah.html", key);

      std::string matching_list;
      std::vector<SBPrefix> prefix_hits;
      std::vector<SBFullHashResult> full_hits;
      GURL gurl(url);
      if (!use_bloom_filter || database->NeedToCheckUrl(gurl)) {
        PerfTimer timer;
        database->ContainsUrl(gurl,
                              &matching_list,
                              &prefix_hits,
                              &full_hits,
                              base::Time::Now());

        int64 time_ms = timer.Elapsed().InMilliseconds();

        DLOG(INFO) << "Read from db in " << time_ms << " ms.";

        db_ms += time_ms;
        keys_from_db++;
      }
    }

    int64 total_ms = total_timer.Elapsed().InMilliseconds();

    DLOG(INFO) << path_.BaseName().value() << " read " << keys_to_read <<
        " entries in " << total_ms << " ms.  "  << keys_from_db <<
        " keys were read from the db, with average read taking " <<
        db_ms / keys_from_db << " ms";
  }

  void BuildBloomFilter() {
    file_util::EvictFileFromSystemCache(path_);
    file_util::Delete(SafeBrowsingDatabase::BloomFilterFilename(path_), false);

    PerfTimer total_timer;

    scoped_ptr<SafeBrowsingDatabase> database(SafeBrowsingDatabase::Create());
    database->SetSynchronous();
    EXPECT_TRUE(database->Init(path_, NULL));

    int64 total_ms = total_timer.Elapsed().InMilliseconds();

    DLOG(INFO) << path_.BaseName().value() <<
        " built bloom filter in " << total_ms << " ms.";
  }

 private:
  FilePath path_;
};

// Adds 100K host records.
TEST(SafeBrowsingDatabase, DISABLED_FillUp100K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing100K")));
  db.Create(100000);
}

// Adds 250K host records.
TEST(SafeBrowsingDatabase, DISABLED_FillUp250K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing250K")));
  db.Create(250000);
}

// Adds 500K host records.
TEST(SafeBrowsingDatabase, DISABLED_FillUp500K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing500K")));
  db.Create(500000);
}

// Reads 500 entries and prints the timing.
TEST(SafeBrowsingDatabase, DISABLED_ReadFrom250K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing250K")));
  db.Read(false);
}

TEST(SafeBrowsingDatabase, DISABLED_ReadFrom500K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing500K")));
  db.Read(false);
}

// Read 500 entries with a bloom filter and print the timing.
TEST(SafeBrowsingDatabase, DISABLED_BloomReadFrom250K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing250K")));
  db.Read(true);
}

TEST(SafeBrowsingDatabase, DISABLED_BloomReadFrom500K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing500K")));
  db.Read(true);
}

// Test how long bloom filter creation takes.
TEST(SafeBrowsingDatabase, DISABLED_BuildBloomFilter250K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing250K")));
  db.BuildBloomFilter();
}

TEST(SafeBrowsingDatabase, DISABLED_BuildBloomFilter500K) {
  SafeBrowsingDatabaseTest db(FilePath(FILE_PATH_LITERAL("SafeBrowsing500K")));
  db.BuildBloomFilter();
}
